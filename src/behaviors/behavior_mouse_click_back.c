/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_mouse_click_back

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zmk/endpoints.h>

#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_mouse_click_back_config {
    uint32_t timeout_ms;
    uint8_t return_layer;
};

struct behavior_mouse_click_back_data {
    struct k_work_delayable work;
    const struct device *dev;
};

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static void mcb_work_handler(struct k_work *work_item) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work_item);
    struct behavior_mouse_click_back_data *data = CONTAINER_OF(dwork, struct behavior_mouse_click_back_data, work);
    const struct device *dev = data->dev;
    const struct behavior_mouse_click_back_config *config = dev->config;

    LOG_DBG("Mouse click back: Work executed, switching to layer %u", config->return_layer);
    int err = zmk_keymap_layer_to(config->return_layer);
    if (err) {
        LOG_ERR("Failed to switch to layer %u: %d", config->return_layer, err);
    }
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param_values[] = {
    {.display_name = "MB1", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = MB1},
    {.display_name = "MB2", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = MB2},
    {.display_name = "MB3", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = MB3},
    {.display_name = "MB4", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = MB4},
    {.display_name = "MB5", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = MB5}};

static const struct behavior_parameter_metadata_set param_metadata_set[] = {{
    .param1_values = param_values,
    .param1_values_len = ARRAY_SIZE(param_values),
}};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = ARRAY_SIZE(param_metadata_set),
    .sets = param_metadata_set,
};

#endif

void mcb_timer_cancel(){
    // Cancel the delayed work if it's scheduled
    if (k_work_delayable_is_pending(&data->work)) {
        k_work_cancel_delayable(&data->work);
        LOG_DBG("Delayed work cancelled on timer cancel for %s.", dev->name);
    }
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d, param1 (button) 0x%02X", event.position, binding->param1);

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (!dev) {
        LOG_ERR("Failed to get device for behavior %s", binding->behavior_dev);
        return ZMK_BEHAVIOR_OPAQUE;
    }
    struct behavior_mouse_click_back_data *data = dev->data;

    // Cancel the delayed work if it's scheduled
    if (k_work_delayable_is_pending(&data->work)) {
        k_work_cancel_delayable(&data->work);
        LOG_DBG("Delayed work cancelled on press for %s.", dev->name);
    }

    int err = zmk_hid_mouse_button_press(binding->param1);
    if (err) {
        LOG_ERR("Failed to press mouse button %d: %d", binding->param1, err);
        return err;
    } 
    // If the button press was successful, we can also send a key event
    zmk_endpoints_send_mouse_report();
    return 0;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d, param1 (button) 0x%02X", event.position, binding->param1);

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
     if (!dev) {
        LOG_ERR("Failed to get device for behavior %s", binding->behavior_dev);
        return ZMK_BEHAVIOR_OPAQUE;
    }
    struct behavior_mouse_click_back_data *data = dev->data;
    const struct behavior_mouse_click_back_config *config = dev->config;

    int err = zmk_hid_mouse_button_release(binding->param1);
    if (err) {
        LOG_ERR("Failed to release mouse button %d: %d", binding->param1, err);
        return err;
    }
    // If the button release was successful, we can also send a key event
    zmk_endpoints_send_mouse_report();

    if (config->timeout_ms > 0) {
        k_work_schedule(&data->work, K_MSEC(config->timeout_ms));
        LOG_DBG("Delayed work scheduled for layer %u, timeout %u ms on %s", config->return_layer, config->timeout_ms, dev->name);
    } else {
        LOG_DBG("Delayed work not scheduled for %s as timeout_ms is 0.", dev->name);
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

static int behavior_mouse_click_back_init(const struct device *dev) {
    struct behavior_mouse_click_back_data *data = dev->data;
    const struct behavior_mouse_click_back_config *config = dev->config;

    data->dev = dev;

    LOG_DBG("Initializing behavior_mouse_click_back '%s' with timeout %ums, return_layer %u",
              dev->name, config->timeout_ms, config->return_layer);

    k_work_init_delayable(&data->work, mcb_work_handler);
    return 0;
}

static const struct behavior_driver_api behavior_mouse_click_back_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define MCB_INST(n)                                                                                \
    static struct behavior_mouse_click_back_data behavior_mouse_click_back_data_##n;                \
    static const struct behavior_mouse_click_back_config behavior_mouse_click_back_config_##n = {  \
        .timeout_ms = DT_INST_PROP(n, timeout_ms),                                                 \
        .return_layer = DT_INST_PROP(n, return_layer),                                             \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_mouse_click_back_init, NULL,                               \
                            &behavior_mouse_click_back_data_##n,                                   \
                            &behavior_mouse_click_back_config_##n, POST_KERNEL,                    \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_mouse_click_back_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MCB_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */

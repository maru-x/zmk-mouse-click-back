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

#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define NO_LAYER_CHANGE_PARAM UINT8_MAX
#define DEFAULT_TIMEOUT_MS 200 // Default timeout in milliseconds if not specified in devicetree

struct behavior_mouse_click_back_config {
    uint32_t timeout_ms;
};

struct behavior_mouse_click_back_data {
    struct k_work_delayable layer_change_work;
    uint8_t target_layer;
    const struct device *dev; // Store device pointer for work handler context
};

static void layer_change_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct behavior_mouse_click_back_data *data =
        CONTAINER_OF(dwork, struct behavior_mouse_click_back_data, layer_change_work);

    if (data->target_layer != NO_LAYER_CHANGE_PARAM && data->target_layer < ZMK_KEYMAP_LAYERS_LEN) {
        LOG_DBG("Changing to layer %d after delay for %s", data->target_layer, data->dev->name);
        zmk_keymap_layer_to(data->target_layer);
    } else {
        LOG_DBG("No valid layer change scheduled or target_layer (%d) is invalid for %s.",
                data->target_layer, data->dev->name);
    }
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param1_values[] = {
    {.display_name = "MB1", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = MB1},
    {.display_name = "MB2", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = MB2},
    {.display_name = "MB3", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = MB3},
    {.display_name = "MB4", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = MB4},
    {.display_name = "MB5", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = MB5}};

static const struct behavior_parameter_value_metadata param2_values[] = {
    {.display_name = "Layer 0", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 0},
    {.display_name = "Layer 1", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 1},
    {.display_name = "Layer 2", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 2},
    {.display_name = "Layer 3", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 3},
    {.display_name = "Layer 4", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 4},
    // Add more layers as needed or make this dynamic if possible
    {.display_name = "No Change", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = NO_LAYER_CHANGE_PARAM},
};


static const struct behavior_parameter_metadata_set param_metadata_set[] = {{
    .param1_values = param1_values,
    .param1_values_len = ARRAY_SIZE(param1_values),
    .param2_values = param2_values,
    .param2_values_len = ARRAY_SIZE(param2_values),
}};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = ARRAY_SIZE(param_metadata_set),
    .sets = param_metadata_set,
};

#endif

static void process_key_state(const struct device *dev, int32_t button_mask, bool pressed) {
    for (int i = 0; i < 5; i++) { // Assuming up to 5 mouse buttons
        if (button_mask & BIT(i)) {
            input_report_key(dev, INPUT_BTN_0 + i, pressed ? 1 : 0, true, K_FOREVER);
        }
    }
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_mouse_click_back_data *data = dev->data;

    LOG_DBG("Pressed: pos %d, button 0x%02X, target_layer %d on %s",
            event.position, binding->param1, binding->param2, dev->name);

    if (k_work_delayable_is_pending(&data->layer_change_work)) {
        k_work_cancel_delayable(&data->layer_change_work);
        LOG_DBG("Cancelled pending layer change for %s", dev->name);
    }

    process_key_state(dev, binding->param1, true);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_mouse_click_back_data *data = dev->data;
    const struct behavior_mouse_click_back_config *config = dev->config;

    LOG_DBG("Released: pos %d, button 0x%02X, target_layer %d on %s, timeout %dms",
            event.position, binding->param1, binding->param2, dev->name, config->timeout_ms);

    process_key_state(dev, binding->param1, false);

    data->target_layer = binding->param2;

    if (data->target_layer != NO_LAYER_CHANGE_PARAM && data->target_layer < ZMK_KEYMAP_LAYERS_LEN) {
        LOG_DBG("Scheduling layer change to %d for %s in %d ms",
                data->target_layer, dev->name, config->timeout_ms);
        k_work_schedule(&data->layer_change_work, K_MSEC(config->timeout_ms));
    } else {
        LOG_DBG("Not scheduling layer change for %s: target_layer %d (NO_LAYER_CHANGE_PARAM is %d, ZMK_KEYMAP_LAYERS_LEN is %d)",
                dev->name, data->target_layer, NO_LAYER_CHANGE_PARAM, ZMK_KEYMAP_LAYERS_LEN);
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

static int behavior_mouse_click_back_init(const struct device *dev) {
    struct behavior_mouse_click_back_data *data = dev->data;
    const struct behavior_mouse_click_back_config *config = dev->config;

    if (data == NULL) {
        LOG_ERR("Device data is NULL for %s", dev->name);
        return -EINVAL;
    }
    if (config == NULL) {
        LOG_ERR("Device config is NULL for %s", dev->name);
        return -EINVAL;
    }
    data->dev = dev; // Store device pointer for work handler
    k_work_init_delayable(&data->layer_change_work, layer_change_work_handler);
    data->target_layer = NO_LAYER_CHANGE_PARAM;
    LOG_DBG("Initialized behavior_mouse_click_back for %s with timeout %ums",
            dev->name, config->timeout_ms);
    return 0;
};

static const struct behavior_driver_api behavior_mouse_click_back_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif
};

#define MCB_INIT_CONFIG(n)                                                                         \
    static const struct behavior_mouse_click_back_config behavior_mouse_click_back_config_##n = {  \
        .timeout_ms = DT_INST_PROP_OR(n, timeout_ms, DEFAULT_TIMEOUT_MS),                          \
    };

#define MCB_INST(n)                                                                                \
    MCB_INIT_CONFIG(n)                                                                             \
    static struct behavior_mouse_click_back_data behavior_mouse_click_back_data_##n;               \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_mouse_click_back_init, NULL,                               \
                            &behavior_mouse_click_back_data_##n,                                   \
                            &behavior_mouse_click_back_config_##n,                                 \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                      \
                            &behavior_mouse_click_back_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MCB_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */

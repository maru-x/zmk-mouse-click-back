/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_mouse_click_back

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/hid.h> // zmk_hid_get_mouse_device() のために必要
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

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

static void process_key_state(const struct device *behavior_device_passed, int32_t button_flags, bool pressed) {
    const struct device *mouse_hid_dev = zmk_hid_get_mouse_device();
    if (!mouse_hid_dev) {
        LOG_ERR("Mouse HID device not found. Ensure CONFIG_ZMK_MOUSE is enabled.");
        return;
    }

    // button_flags はこのループ内で変更されます。これは sync ロジックにとって問題ありません。
    for (int i = 0; i < ZMK_HID_MOUSE_NUM_BUTTONS; i++) {
        if (button_flags & BIT(i)) {
            WRITE_BIT(button_flags, i, 0); // 現在処理中のビットをクリア
            input_report_key(mouse_hid_dev, INPUT_BTN_0 + i, pressed ? 1 : 0, button_flags == 0, K_FOREVER);
            // button_flags が最初に単一のビット（例：MB1）であった場合、
            // ここで button_flags は 0 になり、sync は true になります。
        }
    }
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

    // process_key_state の最初のパラメータ (behavior_device_passed) は、
    // 修正された関数内では直接使用されませんが、API の一貫性のため、または
    // 将来的にビヘイビアが自身のデバイスコンテキストを必要とする可能性のために渡します。
    process_key_state(zmk_behavior_get_binding(binding->behavior_dev), binding->param1, true);

    return ZMK_BEHAVIOR_OPAQUE; // イベントが処理されたことを示す
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

    process_key_state(zmk_behavior_get_binding(binding->behavior_dev), binding->param1, false);

    return ZMK_BEHAVIOR_OPAQUE; // イベントが処理されたことを示す
}

static const struct behavior_driver_api behavior_mouse_click_back_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define MCB_INST(n)                                                                                \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_mouse_click_back_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MCB_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */

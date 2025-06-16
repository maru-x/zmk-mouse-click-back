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
#include <zmk/hid.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param_values[] = {
    {.display_name = "MB1", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 0},
    {.display_name = "MB2", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 1},
    {.display_name = "MB3", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 2},
    {.display_name = "MB4", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 3},
    {.display_name = "MB5", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 4}};

static const struct behavior_parameter_metadata_set param_metadata_set[] = {{
    .param1_values = param_values,
    .param1_values_len = ARRAY_SIZE(param_values),
}};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = ARRAY_SIZE(param_metadata_set),
    .sets = param_metadata_set,
};

#endif

/* ───────────── Per-instance context ───────────── */
struct mcb_ctx {
    struct k_work_delayable back_work;
    uint16_t timeout_ms;
    int8_t return_layer;
};

#define DEFINE_MCB_INST(inst)                                                     \
    static struct mcb_ctx mcb_ctx_##inst;                                         \
                                                                                  \
    static void process_key_state(const struct device *dev, int32_t val, bool pressed) {    \
        for (int i = 0; i < ZMK_HID_MOUSE_NUM_BUTTONS; i++) {                           \
            if (val & BIT(i)) {                 \
                WRITE_BIT(val, i, 0); \
                input_report_key(dev, INPUT_BTN_0 + i, pressed ? 1 : 0, val == 0, K_FOREVER);   \
            }   \
        }   \
    }   \
    static void mcb_back_work_##inst(struct k_work *work) {                       \
        ARG_UNUSED(work);                                                         \
        zmk_keymap_layer_to(mcb_ctx_##inst.return_layer);                         \
    }                                                                             \
                                                                                  \
    static int mcb_pressed_##inst(struct zmk_behavior_binding *binding,           \
                                  struct zmk_behavior_binding_event event) {      \
        LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);   \
        /* ZMK標準のprocess_key_state関数を使用 */         
        process_key_state(zmk_behavior_get_binding(binding->behavior_dev), binding->param1, true);  \

        return 0;                       \

    }                                                                             \
                                                                                  \
    static int mcb_released_##inst(struct zmk_behavior_binding *binding,          \
                                   struct zmk_behavior_binding_event event) {     \
        LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);   \
        /* マウスボタンを離す */                                                   \
        process_key_state(zmk_behavior_get_binding(binding->behavior_dev), \
                                   binding->param1, false);                       \
        /* レイヤー復帰を遅延実行 */                                              \
        k_work_reschedule(&mcb_ctx_##inst.back_work,                              \
                          K_MSEC(mcb_ctx_##inst.timeout_ms));                     \
        return 0;                                                               \
    }                                                                             \
                                                                                  \
    static int mcb_init_##inst(const struct device *dev) {                       \
        ARG_UNUSED(dev);                                                          \
        mcb_ctx_##inst.timeout_ms = DT_INST_PROP_OR(inst, timeout_ms, 500);       \
        mcb_ctx_##inst.return_layer = DT_INST_PROP_OR(inst, return_layer, 0);     \
        /* 重要：遅延ワークの初期化 */                                            \
        k_work_init_delayable(&mcb_ctx_##inst.back_work, mcb_back_work_##inst);   \
        return 0;                                                                 \
    }                                                                             \
                                                                                  \
    static const struct behavior_driver_api mcb_driver_api_##inst = {            \
        .binding_pressed = mcb_pressed_##inst,                                    \
        .binding_released = mcb_released_##inst,                                  \
        IF_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA, (.parameter_metadata = &metadata,)) \
    };                                                                            \
                                                                                  \
    BEHAVIOR_DT_INST_DEFINE(inst, mcb_init_##inst, NULL, NULL, NULL,             \
                           POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,      \
                           &mcb_driver_api_##inst);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_MCB_INST)




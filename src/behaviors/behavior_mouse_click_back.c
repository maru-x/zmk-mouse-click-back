#define DT_DRV_COMPAT zmk_behavior_mouse_click_back

/*
 * behavior,mouse-click-back  —  press: BTN_DOWN / release: BTN_UP + delayed layer_to
 *
 * © 2025 YourName  (MIT)
 */
/* #include <zephyr/sys/printk.h> */ /* 不要なインクルード */
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <zmk/keymap.h>
/* #include <zmk/hid.h> */ /* 不要なインクルード */
#include <zmk/events/mouse_button_state_changed.h>



/* ───────────── Devicetree properties ───────────── */
#define PROP_TIMEOUT_MS(node_id) DT_PROP_OR(node_id, timeout_ms, 500)
#define PROP_RETURN_LAYER(node_id) DT_PROP_OR(node_id, return_layer, 0)

/* ───────────── Per-instance context ───────────── */
struct mcb_ctx {
    struct k_work_delayable back_work;
    uint16_t timeout_ms;
    int8_t   return_layer;
    uint8_t  button_mask;
};

/* マクロでインスタンス展開 */
#define DEFINE_MCB_INST(inst)                                                     \
    static struct mcb_ctx mcb_ctx_##inst;                                         \
                                                                                  \
    static void mcb_back_work_##inst(struct k_work *work) {                       \
        ARG_UNUSED(work);                                                         \
        zmk_keymap_layer_to(mcb_ctx_##inst.return_layer);                         \
    }                                                                             \
                                                                                  \
    static int mcb_pressed_##inst(struct zmk_behavior_binding *binding,           \
                                  struct zmk_behavior_binding_event event) {      \
        mcb_ctx_##inst.button_mask = BIT(binding->param1);                        \
        /* struct zmk_mouse_button_state_changed evt_press = { */                 \
        /*     .buttons   = mcb_ctx_##inst.button_mask, */                        \
        /*     .state     = true, */                                              \
        /*     /.timestamp = k_uptime_get(), */                                   \
        /* }; */                                                                  \
        /* raise_zmk_mouse_button_state_changed(evt_press); */                    \
        raise_zmk_mouse_button_state_changed_from_encoded(                        \
            mcb_ctx_##inst.button_mask,                                           \
            true,                                                                 \
            0 /* k_uptime_get() もしくは明示的に0 */                                 \
        );                                                                        \
        return ZMK_BEHAVIOR_OPAQUE;                                               \
    }                                                                             \
                                                                                  \
    static int mcb_released_##inst(struct zmk_behavior_binding *binding,          \
                                   struct zmk_behavior_binding_event event) {     \
        mcb_ctx_##inst.button_mask = BIT(binding->param1);                        \
        /* struct zmk_mouse_button_state_changed evt_release = { */               \
        /*     .buttons   = mcb_ctx_##inst.button_mask, */                        \
        /*     .state     = false, */                                             \
        /*     /.timestamp = k_uptime_get(), */                                   \
        /* }; */                                                                  \
        /* raise_zmk_mouse_button_state_changed(evt_release); */                  \
        raise_zmk_mouse_button_state_changed_from_encoded(                        \
            mcb_ctx_##inst.button_mask,                                           \
            false,                                                                \
            0 /* k_uptime_get() もしくは明示的に0 */                                 \
        );                                                                        \
        /* レイヤー復帰は従来どおり遅延実行 */                                    \
        k_work_reschedule(&mcb_ctx_##inst.back_work,                              \
                          K_MSEC(mcb_ctx_##inst.timeout_ms));                     \
        return ZMK_BEHAVIOR_OPAQUE;                                               \
    }                                                                             \
                                                                                  \
    static int mcb_init_##inst(const struct device *dev) {                       \
        ARG_UNUSED(dev);                                                          \
        mcb_ctx_##inst.timeout_ms  = PROP_TIMEOUT_MS(DT_DRV_INST(inst));          \
        mcb_ctx_##inst.return_layer = PROP_RETURN_LAYER(DT_DRV_INST(inst));       \
        k_work_init_delayable(&mcb_ctx_##inst.back_work, mcb_back_work_##inst);   \
        return 0;                                                                 \
    }                                                                             \
                                                                                  \
    static const struct behavior_driver_api mcb_driver_api_##inst = {         \
        .binding_pressed = mcb_pressed_##inst,                                    \
        .binding_released = mcb_released_##inst,                                  \
    };                                                                            \
                                                                                  \
    BEHAVIOR_DT_INST_DEFINE(inst, mcb_init_##inst, NULL, NULL, NULL,             \
                           POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,      \
                           &mcb_driver_api_##inst);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_MCB_INST)

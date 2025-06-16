# ライブラリではなく、アプリケーションのソースとして追加
target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_MOUSE_CLICK_BACK app PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/behaviors/behavior_mouse_click_back.c
)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

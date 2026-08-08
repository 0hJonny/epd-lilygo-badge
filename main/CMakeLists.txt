idf_component_register(SRCS "main.cpp"
                            "avatar.c"
                            "app_state.cpp"
                            "gt911_touch.cpp"
                            "graphics_core.cpp"
                            "battery.cpp"
                            "power_manager.cpp"
                            "widgets.cpp"
                            "panels.cpp"
                            "display_engine.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES epd_driver driver lvgl esp_adc_cal
                       EMBED_FILES "ui_font_jp.otf")
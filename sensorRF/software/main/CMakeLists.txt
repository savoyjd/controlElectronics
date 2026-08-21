idf_component_register(
    SRCS "appMain.cpp"
    INCLUDE_DIRS "."
    REQUIRES boardSupport esp_psram esp_system log spi_flash
)

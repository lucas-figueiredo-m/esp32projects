deps_config := \
	/opt/esp/esp-idf/components/app_trace/Kconfig \
	/opt/esp/esp-idf/components/aws_iot/Kconfig \
	/opt/esp/esp-idf/components/bt/Kconfig \
	/opt/esp/esp-idf/components/driver/Kconfig \
	/opt/esp/esp-idf/components/esp32/Kconfig \
	/opt/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/opt/esp/esp-idf/components/esp_http_client/Kconfig \
	/opt/esp/esp-idf/components/ethernet/Kconfig \
	/opt/esp/esp-idf/components/fatfs/Kconfig \
	/opt/esp/esp-idf/components/freertos/Kconfig \
	/opt/esp/esp-idf/components/heap/Kconfig \
	/opt/esp/esp-idf/components/http_server/Kconfig \
	/opt/esp/esp-idf/components/libsodium/Kconfig \
	/opt/esp/esp-idf/components/log/Kconfig \
	/opt/esp/esp-idf/components/lwip/Kconfig \
	/opt/esp/esp-idf/components/mbedtls/Kconfig \
	/opt/esp/esp-idf/components/mdns/Kconfig \
	/opt/esp/esp-idf/components/openssl/Kconfig \
	/opt/esp/esp-idf/components/pthread/Kconfig \
	/opt/esp/esp-idf/components/spi_flash/Kconfig \
	/opt/esp/esp-idf/components/spiffs/Kconfig \
	/opt/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/opt/esp/esp-idf/components/vfs/Kconfig \
	/opt/esp/esp-idf/components/wear_levelling/Kconfig \
	/opt/esp/esp-idf/Kconfig.compiler \
	/opt/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/opt/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/opt/esp/esp32projects/controlledPWM/main/Kconfig.projbuild \
	/opt/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/opt/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;

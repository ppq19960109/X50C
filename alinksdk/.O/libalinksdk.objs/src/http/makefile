MODULE_NAME := src/http
ALL_SUB_DIRS := certs extension external_libs/mbedtls external_libs/nghttp2 src/atm src/coap src/dev_bind src/dev_model src/dev_reset src/dev_sign src/dynamic_register src/http src/http2 src/infra src/mqtt src/ota src/shadow src/wifi_provision wrappers
AR := arm-rockchip-linux-gnueabihf-ar
CC := arm-rockchip-linux-gnueabihf-gcc
CFLAGS := -DALCS_ENABLED -DAWSS_DISABLE_ENROLLEE -DAWSS_DISABLE_REGISTRAR -DAWSS_FRAMEWORKS -DAWSS_SUPPORT_APLIST -DAWSS_SUPPORT_SMARTCONFIG -DAWSS_SUPPORT_SMARTCONFIG_MCAST -DAWSS_SUPPORT_SMARTCONFIG_WPS -DCOAP_PACKET -DCOAP_SERVER -DDEVICE_MODEL_ENABLED -DDEVICE_MODEL_MULTI_THREAD -DDEV_BIND_ENABLED -DDEV_RESET -DDEV_SIGN -DDM_MESSAGE_CACHE_DISABLED -DDYNAMIC_REGISTER -DFORCE_SSL_VERIFY -DHAL_KV -DHTTP_DYNAMIC_REGISTER -DINFRA_AES -DINFRA_CJSON -DINFRA_COMPAT -DINFRA_HTTPC -DINFRA_JSON_PARSER -DINFRA_LIST -DINFRA_LOG -DINFRA_LOG_ALL_MUTED -DINFRA_LOG_MUTE_CRT -DINFRA_LOG_MUTE_DBG -DINFRA_LOG_MUTE_ERR -DINFRA_LOG_MUTE_FLW -DINFRA_LOG_MUTE_INF -DINFRA_LOG_MUTE_WRN -DINFRA_MD5 -DINFRA_NET -DINFRA_REPORT -DINFRA_SHA1 -DINFRA_SHA256 -DINFRA_STRING -DINFRA_TIMER -DLOG_REPORT_TO_CLOUD -DMQTT_AUTO_SUBSCRIBE -DMQTT_COMM_ENABLED -DMQTT_DEFAULT_IMPL -DMQTT_DIRECT -DOTA_ENABLED -DPLATFORM_HAS_DYNMEM -DPLATFORM_HAS_MBEDTLS -DPLATFORM_HAS_STDINT -DSUPPORT_TLS -DTLS_SAVE_TICKET -DWIFI_PROVISION_ENABLED -D_PLATFORM_IS_LINUX_ -Iwrappers -Wall -w
COMP_LIB := libiot_sdk.a
COMP_LIB_COMPONENTS := certs src/coap src/dev_bind src/dev_model src/dev_reset src/dev_sign src/dynamic_register src/infra src/mqtt src/ota src/wifi_provision
CONFIG_VENDOR := arm
CROSS_CANDIDATES := AR CC CXX LD OBJCOPY STRIP
CROSS_PREFIX := arm-rockchip-linux-gnueabihf-
CXX := arm-rockchip-linux-gnueabihf-g++
EXPORT_DIR := 
IMPORT_DIR := tools/prebuilt
INSTALL_DIR := /home/ppq/git/x50bcz/alinksdk/.O/usr
INSTALL_LIB_DIR := /home/ppq/git/x50bcz/alinksdk/.O/usr/lib
KERNEL_DIR := 
LD := arm-rockchip-linux-gnueabihf-ld
LDFLAGS := -ldl -lm -lpthread -lrt
MAKE_ENV_VARS := 
NOEXEC_CMAKE_DIRS := 
OBJCOPY := arm-rockchip-linux-gnueabihf-objcopy
PACKAGE_DIR := /home/ppq/git/x50bcz/alinksdk/packages
RULE_DIR := /home/ppq/git/x50bcz/alinksdk/tools/build-rules
STRIP := arm-rockchip-linux-gnueabihf-strip
SYSROOT_INC := /home/ppq/git/x50bcz/alinksdk/.O/usr/include
TOP_DIR := /home/ppq/git/x50bcz/alinksdk
WIN32_CMAKE_SKIP := 
COMP_LIB_FILES :=  libiot_infra.a  libiot_sign.a  libiot_mqtt.a  libiot_coap.a  libiot_cert.a  libiot_dynreg.a  libiot_reset.a  libiot_ota.a  libiot_alink.a  libiot_bind.a  libiot_awss.a

include $(RULE_DIR)/settings.mk
include $(CONFIG_TPL)

all:

LIBA_TARGET     := libiot_http.a

HDR_REFS        += src/infra

DEPENDS         += wrappers
LDFLAGS         += -liot_sdk -liot_hal -liot_tls

LIB_SRCS_EXCLUDE        := examples/http_example.c
SRCS_http-example   	+= examples/http_example.c

$(call Append_Conditional, TARGET, http-example, HTTP_COMM_ENABLED, BUILD_AOS NO_EXECUTABLES)

env:
	@echo ""
	@printf -- "-----------------------------------------------------------------\n"
	@$(foreach var,$(SHOW_ENV_VARS),$(call Dump_Var,$(var)))
	@printf -- "-----------------------------------------------------------------\n"
	@echo ""

include $(RULE_DIR)/rules.mk

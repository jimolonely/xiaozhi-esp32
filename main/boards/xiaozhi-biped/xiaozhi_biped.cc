/*
    XiaoZhi 双足机器人 - 主板类
    基于 ESP32-S3-DevKitC-1
    包含: INMP441 + MAX98357A + OLED + PCA9685 + HC-SR04
*/

#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

#include "application.h"
#include "biped_main_controller.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/oled_display.h"
#include "mcp_server.h"
#include "system_reset.h"
#include "wifi_board.h"

#define TAG "XiaoZhiBiped"

class XiaoZhiBiped : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    AudioCodec* audio_codec_;
    Display* display_;
    BipedMainController* biped_controller_;
    Button boot_button_;

    void InitializeI2c() {
        i2c_master_bus_config_t bus_cfg = {};
        bus_cfg.i2c_port = I2C_NUM_0;
        bus_cfg.sda_io_num = DISPLAY_I2C_SDA_PIN;
        bus_cfg.scl_io_num = DISPLAY_I2C_SCL_PIN;
        bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_cfg.glitch_ignore_cnt = 7;
        bus_cfg.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus_));
    }

    void InitializeOledDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_i2c_config_t io_config = {};
        io_config.dev_addr = 0x3C;
        io_config.scl_speed_hz = 400 * 1000;
        io_config.control_phase_bytes = 1;
        io_config.dc_bit_offset = 6;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(64),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io, &panel_config, &panel));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new OledDisplay(panel_io, panel, 128, 64, false, false);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        boot_button_.OnDoubleClick([this]() {
            if (biped_controller_ != nullptr) {
                bool current = biped_controller_->IsAutonomousEnabled();
                biped_controller_->SetAutonomousEnabled(!current);
            }
        });
    }

    void InitializeBiped() {
        biped_controller_ = new BipedMainController();
        biped_controller_->Begin();
        biped_controller_->RegisterMcpTools();
    }

    void InitializeAudioCodec() {
        audio_codec_ = new NoAudioCodecSimplex(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_SPK_BCLK_GPIO, AUDIO_SPK_LRCK_GPIO,
            AUDIO_SPK_DOUT_GPIO,
            AUDIO_MIC_SCK_GPIO, AUDIO_MIC_WS_GPIO,
            AUDIO_MIC_DIN_GPIO);
    }

public:
    XiaoZhiBiped()
        : i2c_bus_(nullptr),
          audio_codec_(nullptr),
          display_(nullptr),
          biped_controller_(nullptr),
          boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeOledDisplay();
        InitializeButtons();
        InitializeAudioCodec();
        InitializeBiped();
        ESP_LOGI(TAG, "XiaoZhi双足机器人启动完成");
    }

    ~XiaoZhiBiped() {
        if (biped_controller_ != nullptr) {
            delete biped_controller_;
            biped_controller_ = nullptr;
        }
        if (display_ != nullptr) {
            delete display_;
        }
        if (audio_codec_ != nullptr) {
            delete audio_codec_;
        }
        if (i2c_bus_ != nullptr) {
            i2c_del_master_bus(i2c_bus_);
        }
    }

    std::string GetBoardType() override { return "xiaozhi-biped"; }

    AudioCodec* GetAudioCodec() override { return audio_codec_; }

    Display* GetDisplay() override { return display_; }

    Camera* GetCamera() override { return nullptr; }
};

DECLARE_BOARD(XiaoZhiBiped);
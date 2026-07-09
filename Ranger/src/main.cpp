#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

// I2S Settings
#define I2S_PORT        I2S_NUM_0
#define SAMPLE_RATE     44100

// I2S Pins for MAX98357A
#define I2S_BCLK        27
#define I2S_LRC         26
#define I2S_DOUT        25

// Frequencies for the notes (in Hz)
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392

// "Ode to Joy" Melody Array
int melody[] = {
  NOTE_E4, NOTE_E4, NOTE_F4, NOTE_G4, 
  NOTE_G4, NOTE_F4, NOTE_E4, NOTE_D4, 
  NOTE_C4, NOTE_C4, NOTE_D4, NOTE_E4, 
  NOTE_E4, NOTE_D4, NOTE_D4
};

// Durations for each note
float note_durations[] = {
  1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0,
  1.5, 0.5, 2.0
};

int beat_length_ms = 400; 

void setupI2S() {
    // 1. Standard I2S configuration (No longer using DAC_BUILT_IN)
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, 
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false
    };

    // 2. Map the custom pins
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE // We are not recording audio
    };

    // 3. Install and apply pins
    if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) {
        Serial.println("Failed to install I2S driver");
        while(true); 
    }
    
    if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
        Serial.println("Failed to set I2S pins");
    }
}

void playTone(float frequency, int duration_ms) {
    if (frequency == 0) {
        delay(duration_ms);
        return;
    }

    size_t bytes_written;
    uint16_t sample_val;
    float phase = 0.0;
    float phase_increment = (2.0 * PI * frequency) / SAMPLE_RATE;
    uint16_t buffer[64];

    int total_samples = (SAMPLE_RATE * duration_ms) / 1000;
    int samples_played = 0;

    // To prevent the MAX98357A from clipping and sounding distorted, 
    // we lower the volume multiplier from 255.0 to 100.0. 
    // You can adjust this to change the digital volume.
    float volume = 100.0; 

    while (samples_played < total_samples) {
        int samples_to_write = min(64, total_samples - samples_played);
        
        for (int i = 0; i < samples_to_write; i++) {
            // Generate sine wave
            float sine_wave = (sin(phase) + 1.0) / 2.0; 
            uint8_t dac_val = (uint8_t)(sine_wave * volume);
            
            // Shift to 16-bit
            sample_val = dac_val << 8; 
            buffer[i] = sample_val;

            phase += phase_increment;
            if (phase >= 2.0 * PI) {
                phase -= 2.0 * PI;
            }
        }
        
        i2s_write(I2S_PORT, buffer, samples_to_write * sizeof(uint16_t), &bytes_written, portMAX_DELAY);
        samples_played += samples_to_write;
    }
    
    delay(20);
}

void setup() {
    Serial.begin(115200);
    setupI2S();
    Serial.println("Playing Beethoven's Ode to Joy on MAX98357A...");
}

void loop() {
    int num_notes = sizeof(melody) / sizeof(melody[0]);
    
    for (int i = 0; i < num_notes; i++) {
        int duration = note_durations[i] * beat_length_ms;
        playTone(melody[i], duration);
    }
    
    delay(3000); 
}
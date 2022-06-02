#include <stdio.h>
#include <string.h>
#include "daisy_field.h"
#include "daisysp.h"
#include "fatfs.h"

using namespace daisy;

DaisyField     field;
SdmmcHandler   sdh;
FatFSInterface fsi;
WavPlayer      sampler;
FIL file_object;
WAV_FormatTypeDef header;

unsigned long int sample_size=0;
const long int buffer_size=15000000, snip_size=1000000;
int16_t DSY_SDRAM_BSS buff[buffer_size];
float DSY_SDRAM_BSS sample_space[8][snip_size];

int mode=0;
float speed=1.0;
float first, last;
unsigned long int idx, idx_0, idx_1;
void UpdateDisplay(char c[32],bool um);
char message[32];
class FieldSample{
	public:
		bool Active(bool activate=false){
			if(activate){
				active=true;
				step_location=0.0;
			}
			return(active);
		}
		float Next(){
			if(active){
				// less than the last id and speed = 1
				// if(idx<idx_1 and speed == 1)return(sample_space[key_id][idx++]);
				// speed != 1 and slow
				step_location+=speed;
				idx=int(step_location);
				if(idx+1<=size){
					percent=step_location-float(idx);
					return((1.0-percent)*sample_space[key_id][idx]+percent*sample_space[key_id][idx+1]);
				}
				if(!loop){
					active=false;
				}else{
					idx=0;
					step_location=0;
				}
			}
			return(0.0);
		}
		void CopySample(unsigned long int s, unsigned long int f, int n, float spd){
			key_id=n;
			size=labs(f-s);
			if(size>snip_size)size=snip_size;
			snprintf(message,32,"key=%d spd=%d",n,int(spd));
			UpdateDisplay(message,true);
			for(unsigned long int i=0;i<size;i++){
				if ( spd > 0){
					sample_space[key_id][i]=s162f(buff[s+i]);
				}else{
					sample_space[key_id][i]=s162f(buff[f-i]);
				}
			}
			speed=fabs(spd);
		}


		// private:
		// playback speed
		float speed, step_location, percent;
		// starting index
		int key_id;
		unsigned long int idx, size;
		bool loop, active;
};

size_t num_samples=8, num_active=0;
std::vector<FieldSample> samples(num_samples);


// Use two side buttons to change octaves.
float knob_vals[8];
uint8_t key_vals[16];
float samps=0;

void AudioCallback(AudioHandle::InterleavingInputBuffer  in, AudioHandle::InterleavingOutputBuffer out, size_t size){
	field.ProcessDigitalControls();
	field.ProcessAnalogControls();
	for(int i = 0; i < 8; i++)
	{
		knob_vals[i] = field.GetKnobValue(i);
		key_vals[i]  = field.KeyboardState(i);
		key_vals[i+8]= field.KeyboardState(i+8);
	}
	if(field.GetSwitch(DaisyField::SW_1)->RisingEdge())mode=0;
	if(field.GetSwitch(DaisyField::SW_2)->RisingEdge())mode=1;
	samps=0.0;
	num_active=0;

	for(size_t i = 0; i < size; i += 2) {
		if(mode==1){
			for(size_t j = 0; j < num_samples; j++){
				num_active+=samples[j].Active(field.KeyboardRisingEdge(j));
				samps+=samples[j].Next();
			}
			samps=samps/float(num_active);
		}else{
			if(idx_1>idx_0){
				samps=s162f(buff[idx++]);
				if(idx>=idx_1)idx=idx_0;
				if(idx<=idx_0)idx=idx_0;
			}else{
				samps=s162f(buff[idx--]);
				if(idx<=idx_0)idx=idx_1;
				if(idx>idx_1)idx=idx_1;
			}
		}
		out[i] = out[i + 1] = samps;
	}
}



unsigned long int display_time=0, led_time=0, adsr_time=0, wf_time=0, octave_time=0, copy_time=0;

void UpdateKeyboardLeds(){
	uint8_t led_grp[] = {
		DaisyField::LED_KEY_A1,
		DaisyField::LED_KEY_A2,
		DaisyField::LED_KEY_A3,
		DaisyField::LED_KEY_A4,
		DaisyField::LED_KEY_A5,
		DaisyField::LED_KEY_A6,
		DaisyField::LED_KEY_A7,
		DaisyField::LED_KEY_A8,
		DaisyField::LED_KEY_B1,
		DaisyField::LED_KEY_B2,
		DaisyField::LED_KEY_B3,
		DaisyField::LED_KEY_B4,
		DaisyField::LED_KEY_B5,
		DaisyField::LED_KEY_B6,
		DaisyField::LED_KEY_B7,
		DaisyField::LED_KEY_B8};
	uint8_t led_knob[] = {
		DaisyField::LED_KNOB_1,
		DaisyField::LED_KNOB_2,
		DaisyField::LED_KNOB_3,
		DaisyField::LED_KNOB_4,
		DaisyField::LED_KNOB_5,
		DaisyField::LED_KNOB_6,
		DaisyField::LED_KNOB_7,
		DaisyField::LED_KNOB_8};
	if(System::GetNow()+100>led_time){
		led_time=System::GetNow()+100;
		for(uint8_t i=0;i<8;i++){
			field.led_driver.SetLed(led_knob[i], float(knob_vals[i]));
			field.led_driver.SetLed(led_grp[i], float(key_vals[i]));
			field.led_driver.SetLed(led_grp[i+8], float(key_vals[i+8]));
		}
	}
	field.led_driver.SwapBuffersAndTransmit();
}

void UpdateDisplay(char c[32], bool message_only){
	//display
	char line1[32], line2[32], line3[32];
	if(message_only){
		field.display.Fill(false);
		field.display.SetCursor(0,0);
		field.display.WriteString(c, Font_7x10 , true);
	}

	if(System::GetNow()>display_time){
		display_time=System::GetNow()+1000;
		snprintf(line1,32,"%ld %ld\n",idx_0,idx_1);
		snprintf(line2,32,"%ld %ld",samples[0].idx,samples[0].size);
		snprintf(line3,32,"%2d %2d",int(samples[0].speed*10),int(speed*10));
		field.display.Fill(false);
		field.display.SetCursor(0,0);
		field.display.WriteString(line1, Font_7x10 , true);
		field.display.SetCursor(0,20);
		field.display.WriteString(line2, Font_7x10 , true);
		field.display.SetCursor(0,35);
		field.display.WriteString(line3, Font_7x10 , true);
		field.display.SetCursor(0,45);
		field.display.WriteString(c, Font_7x10 , true);
	}
	field.display.Update();
}

void UpdateControls(){
	speed=float(int(20.0*knob_vals[6]))/10.0;
	if(speed==0)speed=0.1;
	if(knob_vals[7]<0.5)speed=-speed;
	idx_0=int(knob_vals[1]*sample_size/100)*100;
	idx_0=idx_0+int(knob_vals[2]*sample_size/10);
	idx_1=int(knob_vals[3]*sample_size/100)*100;
	if(idx_1>=sample_size)idx_1=sample_size-1;
	for(size_t j = 0; j < num_samples; j++){
		if(key_vals[j]) {
			if(mode==0){
				if(System::GetNow()>copy_time){
					copy_time=System::GetNow()+100;
					samples[j].CopySample(idx_0,idx_1,j,speed);
				}
			}else{
				samples[j].Active(true);
				if(key_vals[j+8])samples[j].loop=true;
			}

		}else{
			if(key_vals[j+8]){
				samples[j].loop=false;
			}
		}
	}

}

void sdcard_init() {
	daisy::SdmmcHandler::Config sdconfig;
	sdconfig.Defaults(); 
	sdconfig.speed           = daisy::SdmmcHandler::Speed::VERY_FAST; 
	sdconfig.width           = daisy::SdmmcHandler::BusWidth::BITS_4;
	sdh.Init(sdconfig);
	fsi.Init(FatFSInterface::Config::MEDIA_SD);
	f_mount(&fsi.GetSDFileSystem(), "/", 1);
	// sampler.Init(fsi.GetSDPath());
	if (f_open(&file_object, "mono.wav", FA_READ) == FR_OK) {
		UINT bytes_read = 0;
		f_read(&file_object, &header, sizeof(header), &bytes_read);
		unsigned long int file_size = header.FileSize;
		sample_size=header.FileSize/2;
		if ( sample_size > buffer_size){
			file_size=buffer_size;
			sample_size=buffer_size/2;
		}
		f_read(&file_object, &buff,  file_size, &bytes_read);
	}
	idx=0;
	idx_0=0;
	idx_1=sample_size;
	for(unsigned long int i=0;i<sample_size;i++)buff[i]=buff[i+sizeof(header)];

}

int main(void) {
	size_t blocksize = 4;
	// set up the field
	field.Init();
	// set up the sd card handler class to defaults
	sdcard_init();
	field.StartAdc();
	field.SetAudioBlockSize(blocksize);
	field.StartAudio(AudioCallback);
	for(;;)
	{
		UpdateControls();
		snprintf(message,32,"mode=%d",mode);
		UpdateDisplay(message,false);
		UpdateKeyboardLeds();
	}
}

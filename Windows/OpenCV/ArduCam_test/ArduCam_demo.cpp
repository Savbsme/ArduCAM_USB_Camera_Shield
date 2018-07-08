// ArduCam_test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "ArduCamlib.h"


#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <thread>
#include <iostream>
#include <istream>
#include <string> 

#define USB3
#define MONO

volatile bool _running = true;

cv::Mat BytestoMat(Uint8* bytes, int width, int height)
{
	cv::Mat image = cv::Mat(height, width, CV_8UC1, bytes);
	return image;
}

void camera_init(ArduCamHandle handle)
{
	Uint8 u8Buf[10];

#ifdef USB2
	u8Buf[0] = 0x05;
	ArduCam_boardConfig(handle, 0xD7, 0x4600, 0x0100, 0x01, u8Buf);
	u8Buf[0] = 0x03;
	u8Buf[1] = 0x04;
	u8Buf[2] = 0x0C;
	ArduCam_boardConfig(handle, 0xF6, 0x0000, 0x0000, 0x03, u8Buf);
#endif

#ifdef USB3
	ArduCam_boardConfig(handle, 0xF3, 0x0000, 0x0000, 0x00, u8Buf);
	ArduCam_boardConfig(handle, 0xF9, 0x0000, 0x0000, 0x00, u8Buf);
#endif

	ArduCam_writeSensorReg(handle, 0x3028, 0x0010);		// ROW_SPEED = 16
	ArduCam_writeSensorReg(handle, 0x302E, 0x0002);		// PRE_PLL_CLK_DIV = 2 (N)

#ifdef USB2
	ArduCam_writeSensorReg(handle, 0x3030, 0x0018);		// PLL_MULTIPLIER = 47 (M)
#else
	ArduCam_writeSensorReg(handle, 0x3030, 0x002F);		// PLL_MULTIPLIER = 47 (M)
#endif

	ArduCam_writeSensorReg(handle, 0x302C, 0x0001);		// VT_SYS_CLK_DIV = 1  (P1)
	ArduCam_writeSensorReg(handle, 0x302A, 0x0008);		// VT_PIX_CLK_DIV = 8 (P2)
	ArduCam_writeSensorReg(handle, 0x3032, 0x0000);		// DIGITAL_BINNING = 0
	ArduCam_writeSensorReg(handle, 0x30B0, 0x0480);		// DIGITAL_TEST: 0x30B0[10] = 1 (enable short line length of 1388); 0x30B0[7] = 1 (monochrome);
	ArduCam_writeSensorReg(handle, 0x301A, 0x19D8); 	// RESET_REGISTER = 4316
	ArduCam_writeSensorReg(handle, 0x300C, 0x0672); 	// LINE_LENGTH_PCK = 1650
	ArduCam_writeSensorReg(handle, 0x3002, 0x0000); 	// Y_ADDR_START = 0
	ArduCam_writeSensorReg(handle, 0x3004, 0x0000); 	// X_ADDR_START = 0
	ArduCam_writeSensorReg(handle, 0x3006, 0x03BF); 	// Y_ADDR_END = 959
	ArduCam_writeSensorReg(handle, 0x3008, 0x04FF); 	// X_ADDR_END = 1279
	ArduCam_writeSensorReg(handle, 0x300A, 0x03DE); 	// FRAME_LENGTH_LINES = 990
	ArduCam_writeSensorReg(handle, 0x3012, 0x0040); 	// COARSE_INTEGRATION_TIME = 64
	ArduCam_writeSensorReg(handle, 0x3014, 0x00C0); 	// FINE_INTEGRATION_TIME = 192
	ArduCam_writeSensorReg(handle, 0x30A6, 0x0001); 	// Y_ODD_INC = 1
	ArduCam_writeSensorReg(handle, 0x308C, 0x0000); 	// Y_ADDR_START_CB = 0
	ArduCam_writeSensorReg(handle, 0x308A, 0x0000); 	// X_ADDR_START_CB = 0
	ArduCam_writeSensorReg(handle, 0x3090, 0x03BF); 	// Y_ADDR_END_CB = 959
	ArduCam_writeSensorReg(handle, 0x308E, 0x04FF); 	// X_ADDR_END_CB = 1279
	ArduCam_writeSensorReg(handle, 0x30AA, 0x03DE); 	// FRAME_LENGTH_LINES_CB = 990
	ArduCam_writeSensorReg(handle, 0x3016, 0x0064); 	// COARSE_INTEGRATION_TIME_CB = 100
	ArduCam_writeSensorReg(handle, 0x3018, 0x00C0); 	// FINE_INTEGRATION_TIME_CB = 192
	ArduCam_writeSensorReg(handle, 0x30A8, 0x0001); 	// Y_ODD_INC_CB = 1
	ArduCam_writeSensorReg(handle, 0x3040, 0x0000); 	// READ_MODE = 0
	ArduCam_writeSensorReg(handle, 0x3064, 0x1982); 	// EMBEDDED_DATA_CTRL = 6530
	ArduCam_writeSensorReg(handle, 0x31C6, 0x8000); 	// HISPI_CONTROL_STATUS = 32768 (DEFAULT)
	ArduCam_writeSensorReg(handle, 0x3100, 0x0000);
	ArduCam_writeSensorReg(handle, 0x305E, 0x0030);
	ArduCam_writeSensorReg(handle, 0x3046, 0x0100);

#ifdef MONO
	//do nothing
#else
	ArduCam_writeSensorReg(handle, 0x3056, 74);
	ArduCam_writeSensorReg(handle, 0x3058, 112);
	ArduCam_writeSensorReg(handle, 0x305a, 112);
	ArduCam_writeSensorReg(handle, 0x305c, 74);
#endif 

	ArduCam_writeSensorReg(handle, 0x3064, 0x1802);		// diable embedded data
}

bool camera_initFromFile(std::string filename, ArduCamHandle &cameraHandle, ArduCamCfg &cameraCfg) {
	cv::FileStorage cfg;
	if (cfg.open(filename, cv::FileStorage::Mode::READ)) {
		cv::FileNode cp = cfg["camera_param"];
		int value;
		std::string hexStr;

		cp["emI2cMode"] >> value;
		switch (value) {
		case 0: cameraCfg.emI2cMode = I2C_MODE_8_8; break;
		case 1: cameraCfg.emI2cMode = I2C_MODE_8_16; break;
		case 2: cameraCfg.emI2cMode = I2C_MODE_16_8; break;
		case 3: cameraCfg.emI2cMode = I2C_MODE_16_16; break;
		default: break;
		}

		cp["emImageFmtMode"] >> value;
		switch (value) {
		case 0: cameraCfg.emImageFmtMode = FORMAT_MODE_RAW; break;
		case 1: cameraCfg.emImageFmtMode = FORMAT_MODE_RGB; break;
		case 2: cameraCfg.emImageFmtMode = FORMAT_MODE_YUV; break;
		case 3: cameraCfg.emImageFmtMode = FORMAT_MODE_JPG; break;
		case 4: cameraCfg.emImageFmtMode = FORMAT_MODE_MON; break;
		case 5: cameraCfg.emImageFmtMode = FORMAT_MODE_RAW_D; break;
		case 6: cameraCfg.emImageFmtMode = FORMAT_MODE_MON_D; break;
		default: break;
		}

		cp["u32Height"] >> value; cameraCfg.u32Height = value;
		cp["u32Width"] >> value; cameraCfg.u32Width = value;
		cp["u32I2cAddr"] >> hexStr; cameraCfg.u32I2cAddr = std::stoul(hexStr, nullptr, 16);
		cp["u8PixelBits"] >> value; cameraCfg.u8PixelBits = value;
		cp["u8PixelBytes"] >> value; cameraCfg.u8PixelBytes = value;
		cp["u32TransLvl"] >> value; cameraCfg.u32TransLvl = value;

		if (ArduCam_autoopen(cameraHandle, &cameraCfg) == USB_CAMERA_NO_ERROR) {
			cv::FileNode bp = cfg["board_param"];
			for (int i = 0; i < bp.size(); i++) {
				uint8_t u8Buf[10];
				for (int j = 0; j < bp[i][4].size(); j++)
					bp[i][4][j] >> u8Buf[j];

				bp[i][0] >> hexStr;
				Uint8 u8Command = std::stoul(hexStr, nullptr, 16);
				bp[i][1] >> hexStr;
				Uint16 u16Value = std::stoul(hexStr, nullptr, 16);
				bp[i][2] >> hexStr;
				Uint16 u16Index = std::stoul(hexStr, nullptr, 16);
				bp[i][3] >> hexStr;
				Uint32 u32BufSize = std::stoul(hexStr, nullptr, 16);
				ArduCam_boardConfig(cameraHandle, u8Command, u16Index, u16Value, u32BufSize, u8Buf);
			}
			cv::FileNode rp = cfg["register_param"];
			for (int i = 0; i < rp.size(); i++) {
				rp[i][0] >> hexStr;
				Uint32 addr = std::stoul(hexStr, nullptr, 16);
				rp[i][1] >> hexStr;
				Uint32 val = std::stoul(hexStr, nullptr, 16);
				ArduCam_writeSensorReg(cameraHandle, addr, val);
			}
		}
		else {
			std::cout << "Cannot open camera" << std::endl;
			cfg.release();
			return false;
		}

		cfg.release();
		return true;
	}
	else {
		std::cout << "Cannot find configuration file" << std::endl;
		return false;
	}
}

void captureImage(ArduCamHandle handle) {
	Uint32 rtn_val = ArduCam_beginCaptureImage(handle);
	if ( rtn_val == USB_CAMERA_USB_TASK_ERROR) {
		std::cout << "Error beginning capture, rtn_val = " << rtn_val << std::endl;
		return;
	}
	else {
		std::cout << "Capture began, rtn_val = " << rtn_val << std::endl;
	}

	while (_running) {
		rtn_val = ArduCam_captureImage(handle);
		if ( rtn_val == USB_CAMERA_USB_TASK_ERROR) {
			std::cout << "Error capture image, rtn_val = " << rtn_val << std::endl;
			return;
		}
		else {
			//std::cout << "Image captured, rtn_val = " << rtn_val << std::endl;
		}
	}

	ArduCam_endCaptureImage(handle);
	std::cout << "Capture thread stopped" << std::endl;
}

void readImage(ArduCamHandle handle) {
	ArduCamOutData* frameData;
	while (_running) {
		if (ArduCam_availableImage(handle) > 0) {
			Uint32 rtn_val = ArduCam_readImage(handle, frameData);
			if ( rtn_val == USB_CAMERA_NO_ERROR) {
				cv::Mat rawImage = BytestoMat(frameData->pu8ImageData, 1280, 964);

				#ifdef MONO
				//do nothing
				#else
				cv::Mat rgbImage;
				cv::cvtColor(rawImage, rgbImage, CV_BayerGR2RGB);
				#endif 

				ArduCam_del(handle);
				//std::cout << "Image read, rtn_val = " << rtn_val << std::endl;

				#ifdef MONO
				cv::imshow("ArduCam Test", rawImage);
				#else
				cv::imshow("ArduCam Test", rgbImage);
				#endif

				cv::waitKey(2);
			}
			else {
				std::cout << "Error reading image, rtn_val = " << rtn_val << std::endl;
			}
		}
		else {
			//std::cout << "No data in the stack" << std::endl;
		}
	}

	std::cout << "Read thread stopped" << std::endl;
}



int main()
{
	ArduCamHandle cameraHandle;
	ArduCamCfg cameraCfg;
	
	/**
		Configure camera from file
	**/

	if (camera_initFromFile("ArduCamCfg.json", cameraHandle, cameraCfg)) {
		std::thread captureThread(captureImage, cameraHandle);
		std::thread readThread(readImage, cameraHandle);
		cv::namedWindow("ArduCam Test", cv::WINDOW_AUTOSIZE);
		while (cv::waitKey(1000) != 27) {
			// do nothing
		}
		_running = false;
		readThread.join();
		captureThread.join();
		cv::destroyAllWindows();
		ArduCam_close(cameraHandle);
	}

	std::cout << std::endl << "Press ENTER to exit..." << std::endl;
	std::string key;
	std::getline(std::cin,key);
    return 0;
}



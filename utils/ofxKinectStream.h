#pragma once

#include "ofMain.h"

#include "ofxTimebasedStream.h"

namespace ofxKinectStream
{

class Recorder : public ofThread, public ofxTimebasedStream::Writer
{
public:

	Recorder()
	{
		colorImage.setUseTexture(false);
		colorImage.allocate(640, 480, OF_IMAGE_COLOR);

		depthImage.setUseTexture(false);
		depthImage.allocate(640, 480, OF_IMAGE_GRAYSCALE);

		recording = false;
	}

	~Recorder()
	{
		stop();
	}

	void start(string filename)
	{
		stop();

		filename = ofToString(filename);
		ofxTimebasedStream::Writer::open(filename);

		recordingStartTime = ofGetElapsedTimef();
		frameNum = 0;

		startThread(true, false);

		recording = true;
	}

	void stop()
	{
		recording = false;

		if (isThreadRunning())
			waitForThread();

		ofxTimebasedStream::Writer::close();
	}

	inline bool isRecording()
	{
		return recording;
	}

	inline int getFrameNum() const
	{
		return frameNum;
	}

	void addFrame(unsigned char *rgb, unsigned short *raw_depth)
	{
		if (!isRecording()) return;

		if (lock())
		{
			writeTimestamp = ofGetElapsedTimef() - recordingStartTime;

			colorImage.setFromPixels(rgb, 640, 480, OF_IMAGE_COLOR);
			depthImage.setFromPixels(raw_depth, 640, 480, OF_IMAGE_GRAYSCALE);

			if (hasNewFrame) ofLogError("ofxKinectStream::Recorder", "drop frame");
			hasNewFrame = true;

			unlock();
		}
	}

protected:

	int frameNum;

	bool hasNewFrame;
	bool recording;

	float recordingStartTime;

	float writeTimestamp;
	ofImage colorImage;
	ofShortImage depthImage;

	void threadedFunction()
	{
		while (isThreadRunning())
		{
			if (hasNewFrame && lock())
			{
				size_t len = 0;
				ofBuffer buffer;
				ostringstream ost;
				Poco::BinaryWriter writer(ost, Poco::BinaryWriter::NATIVE_BYTE_ORDER);

				ofSaveImage(colorImage.getPixelsRef(), buffer, OF_IMAGE_FORMAT_JPEG);
				len = buffer.size();

				writer << len;
				writer << buffer;

				ofSaveImage(depthImage.getPixelsRef(), buffer, OF_IMAGE_FORMAT_TIFF);
				len = buffer.size();

				writer << len;
				writer << buffer;

				write(writeTimestamp, ost.str());

				hasNewFrame = false;
				frameNum++;

				unlock();
			}

			ofSleepMillis(1);
		}
	}

};

class Player : public ofxTimebasedStream::Reader
{
public:

	ofImage colorImage;
	ofShortImage depthImage;

	Player()
	{
		frameNum = 0;
		playing = false;
	}

	~Player()
	{
		close();
	}

	void open(string path)
	{
		ofxTimebasedStream::Reader::open(path);

		if (*this)
		{
			colorImage.allocate(640, 480, OF_IMAGE_COLOR);
			depthImage.allocate(640, 480, OF_IMAGE_GRAYSCALE);
		}

		rewind();
	}

	void play()
	{
		playing = true;
	}

	void stop()
	{
		playing = false;
	}

	inline bool isPlaying() const
	{
		return playing;
	}

	void rewind()
	{
		ofxTimebasedStream::Reader::rewind();

		playStartTime = ofGetElapsedTimef();
		playHeadTime = 0;
		frameNum = 0;
	}

	inline bool isFrameNew() const
	{
		return frameNew;
	}

	inline int getFrameNum() const
	{
		return frameNum;
	}

	void update()
	{
		if (*this && playing)
		{
			//
			// read stream
			//
			
			string data;
			playHeadTime += ofGetLastFrameTime();
			
			while (playHeadTime > getTimestamp())
			{
				// skip to seek head
				if (!nextFrame(data)) break;
				frameNum++;
			}

			if (isEof())
			{
				if (isLoop())
				{
					rewind();
				}
				else
				{
					playing = false;
				}
			}
			
			if (!data.empty())
			{
				istringstream ist(data);
				Poco::BinaryReader reader(ist, Poco::BinaryReader::NATIVE_BYTE_ORDER);
				
				size_t len = 0;
				string buffer;
				ofBuffer ofbuf;

				reader >> len;
				reader.readRaw(len, buffer);
				
				ofbuf.set(buffer.data(), buffer.size());
				
				ofLoadImage(colorImage.getPixelsRef(), ofbuf);
				colorImage.update();
				
				reader >> len;
				reader.readRaw(len, buffer);
				
				ofbuf.set(buffer.data(), buffer.size());
				
				ofLoadImage(depthImage.getPixelsRef(), ofbuf);
				depthImage.update();
			}
		}
	}

	void draw(int x, int y)
	{
		colorImage.draw(x, y);
	}

	void drawDepth(int x, int y)
	{
		depthImage.draw(x, y);
	}

	void setLoop(bool yn){ loop = yn; }
	bool isLoop() const { return loop; }

private:

	bool playing, frameNew;
	float playStartTime;
	float playHeadTime;
	int frameNum;
	bool loop;

};

}

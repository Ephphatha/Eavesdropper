/*
 *  The MIT License
 *
 *  Copyright 2011 Andrew James <ephphatha@thelettereph.com>.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */
#include <iostream>
#include <fstream>
#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>

#pragma warning(push)
#pragma warning(disable:4244) // Implicit type conversion in boost header
#include <boost/thread/thread.hpp>
#pragma warning(pop)

#include <lame.h>

#ifdef _DEBUG
#pragma comment(lib, "libmp3lame-static.lib")
#else
#pragma comment(lib, "libmp3lame.lib")
#endif
#pragma comment(lib, "mpglib-static.lib")

//#define SFML_DYNAMIC

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#ifdef _DEBUG
#ifdef SFML_DYNAMIC
#pragma comment(lib, "sfml-audio-d.lib")
#pragma comment(lib, "sfml-graphics-d.lib")
#pragma comment(lib, "sfml-window-d.lib")
#pragma comment(lib, "sfml-system-d.lib")
#else
#pragma comment(lib, "sfml-audio-s-d.lib")
#pragma comment(lib, "sfml-graphics-s-d.lib")
#pragma comment(lib, "sfml-window-s-d.lib")
#pragma comment(lib, "sfml-system-s-d.lib")
#endif
#else
#ifdef SFML_DYNAMIC
#pragma comment(lib, "sfml-audio.lib")
#pragma comment(lib, "sfml-graphics.lib")
#pragma comment(lib, "sfml-window.lib")
#pragma comment(lib, "sfml-system.lib")
#else
#pragma comment(lib, "sfml-audio-s.lib")
#pragma comment(lib, "sfml-graphics-s.lib")
#pragma comment(lib, "sfml-window-s.lib")
#pragma comment(lib, "sfml-system-s.lib")
#endif
#endif

class OnDiskBufferRecorder : public sf::SoundRecorder
{
public:
  OnDiskBufferRecorder();
  ~OnDiskBufferRecorder();

  bool OnProcessSamples(const sf::Int16 *samples, std::size_t samplesCount);

private:
  boost::posix_time::ptime timestamp;
  std::ofstream file;
  std::string extension;

  static const int MP3SIZE = 8192;
  static const int PCMSIZE = 8192;
  unsigned char mp3Buffer[MP3SIZE];
  short int pcmBuffer[PCMSIZE * sizeof(short int)];

  lame_t lame;

  bool OnStart();
  void OnStop();
};

OnDiskBufferRecorder::OnDiskBufferRecorder()
  : extension(".mp3"),
  lame(lame_init())
{
  lame_set_in_samplerate(lame, 44100);
  lame_set_VBR(lame, vbr_off);
  lame_set_brate(lame, 64);
  lame_set_mode(lame, MONO);
  lame_init_params(lame);
}

OnDiskBufferRecorder::~OnDiskBufferRecorder()
{
  lame_close(lame);
}

bool OnDiskBufferRecorder::OnProcessSamples(const sf::Int16 *samples, std::size_t samplesCount)
{
#ifdef _DEBUG
  std::cout << "Captured " << samplesCount << " samples" << std::endl;
#endif

  int write;

  boost::posix_time::ptime current = boost::posix_time::microsec_clock::local_time();

  if((this->timestamp.time_of_day().hours() < current.time_of_day().hours()))
  {
    write = lame_encode_flush(this->lame, this->mp3Buffer, OnDiskBufferRecorder::MP3SIZE);
    this->file.write(reinterpret_cast<const char*>(this->mp3Buffer), write);
    this->file.close();
    this->timestamp = current;
    this->file.open(boost::posix_time::to_iso_string(timestamp) + extension,
                    std::ofstream::out | std::ofstream::binary);
  }

  write = lame_encode_buffer(this->lame, samples, samples, samplesCount, this->mp3Buffer, OnDiskBufferRecorder::MP3SIZE);
#ifdef _DEBUG
  std::cout << write << " bytes written to the lame buffer, writing to file." << std::endl;
#endif
  this->file.write(reinterpret_cast<const char*>(this->mp3Buffer), write);

  return true;
}

bool OnDiskBufferRecorder::OnStart()
{
  if (!sf::SoundRecorder::CanCapture())
  {
    std::cerr << "Unable to capture audio." << std::endl;
    return false;
  }

  this->timestamp = boost::posix_time::microsec_clock::local_time();
  this->file.open(boost::posix_time::to_iso_string(timestamp) + extension,
                  std::ofstream::out | std::ofstream::binary);

  std::cout << "Recording." << std::endl;
  return true;
}

void OnDiskBufferRecorder::OnStop()
{
  std::cout << "Closing file." << std::endl;
  int write = lame_encode_flush(this->lame, this->mp3Buffer, OnDiskBufferRecorder::MP3SIZE);
  this->file.write(reinterpret_cast<const char*>(this->mp3Buffer), write);
  this->file.close();
  std::cout << "No longer recording." << std::endl;
}

void record(const std::string &extension)
{
  std::auto_ptr<sf::SoundBufferRecorder> recorder[2] = 
  {
    std::auto_ptr<sf::SoundBufferRecorder>(new sf::SoundBufferRecorder),
    std::auto_ptr<sf::SoundBufferRecorder>(new sf::SoundBufferRecorder)
  };

  int active = 0;

  if (!recorder[active]->CanCapture())
  {
    std::cerr << "You need an input device jackass." << std::endl;
    return;
  }
  
  boost::posix_time::ptime timestamp(boost::posix_time::microsec_clock::local_time());

  std::cout << "Recording on buffer " << active << std::endl;
  recorder[active]->Start();
  
  bool running = true;
  while (running)
  {
    try
    {
      do
      { // Sleep until we would pass the hour.
        boost::this_thread::sleep(boost::posix_time::time_duration(timestamp.time_of_day().hours() + 1, 0, 0) - timestamp.time_of_day());
      } // Keep looping till we pass the hour (in case we get woken early).
      while (!(timestamp.time_of_day().hours() < boost::posix_time::microsec_clock::local_time().time_of_day().hours()));
    }
    catch (boost::thread_interrupted const&)
    {
      running = false;
    }

    int next = (active + 1) % 2;
    std::cout << "Stopping buffer " << active << ", preparing to write to file." << std::endl;
    recorder[active]->Stop();

    if (running)
    {
      std::cout << "Recording on buffer " << next << std::endl;
      recorder[next]->Start();
    }

    std::string filename = boost::posix_time::to_iso_string(timestamp) + extension;
    std::cout << "Writing buffer to file with filename: " << filename << std::endl;
    recorder[active]->GetBuffer().SaveToFile(filename);

    active = next;
    timestamp = boost::posix_time::microsec_clock::local_time();
  }
}

int main(int, char**)
{
  sf::RenderWindow window(sf::VideoMode(200, 200), "Eavesdropper", sf::Style::Close);

  sf::Image image;
  image.LoadFromFile("background.jpg");

  sf::Sprite sprite;
  sprite.SetImage(image);

  window.Draw(sprite);
  window.Display();

  //std::string extension = ".wav";

  //boost::thread recordThread(record, extension);

  OnDiskBufferRecorder recorder;

  recorder.Start();

  bool running = true;
  while (running)
  {
    sf::Event event;

    while (window.GetEvent(event))
    {
      if ((event.Type == sf::Event::Closed) ||
        (event.Type == sf::Event::KeyPressed) && (event.Key.Code == sf::Key::Escape))
      {
        running = false;
      }
    }
  }

  recorder.Stop();
  //recordThread.interrupt();
  //recordThread.join();

  return 0;
}

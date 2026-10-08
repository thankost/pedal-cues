    // Song Builder audio: the metronome and the backing track, rendered block by block like the audio thread does.
    {
        constexpr double rate = 48000.0;
        constexpr int block = 512;
        SongAudio audio;
        audio.prepare (rate, block);
        juce::AudioBuffer<float> buffer (2, block);
        juce::int64 now = 0;
        // Renders `seconds` and returns the loudest sample after `quietFrom` seconds into it.
        const auto run = [&] (double seconds, double quietFrom)
        {
            float loudest = 0.0f;
            const auto blocks = (int) std::ceil (seconds * rate / block);
            for (int b = 0; b < blocks; ++b)
            {
                buffer.clear();
                audio.render (buffer, block, now);
                if ((double) b * block / rate >= quietFrom)
                    loudest = juce::jmax (loudest, buffer.getMagnitude (0, block));
                now += block;
            }
            return loudest;
        };
        songs::ClickSound click;
        audio.start (now, click, 0, 0.8f);
        std::vector<SongAudio::Click> clicks;
        for (int i = 0; i < 20; ++i)
            clicks.push_back ({ i * 0.5, i % 4 == 0, false });
        audio.addClicks (clicks);
        CHECK (run (1.2, 0.0) > 0.05f);    // it clicks
        audio.stop();
        CHECK (run (2.0, 0.15) < 1.0e-6f);  // stopped: at most the click that was sounding finishes, then silence
        // Started again from scratch: only the new clicks sound, at their new times.
        audio.start (now, click, 0, 0.8f);
        audio.addClicks ({ { 1.0, true, false } });
        CHECK (run (0.9, 0.0) < 1.0e-6f && run (0.3, 0.0) > 0.05f);

        // With a backing track loaded, Stop is immediate too (it once waited a second for the player, and the clicks
        // scheduled in that second still sounded).
        const auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pedalcues-backing-test.wav");
        {
            wav.deleteFile();
            juce::WavAudioFormat format;
            std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (new juce::FileOutputStream (wav), rate, 1, 16, {}, 0));
            juce::AudioBuffer<float> tone (1, (int) rate * 4);
            for (int i = 0; i < tone.getNumSamples(); ++i)
                tone.setSample (0, i, 0.3f * std::sin (i * 0.05f));
            writer->writeFromAudioSampleBuffer (tone, 0, tone.getNumSamples());
        }
        CHECK (audio.loadBacking (wav));
        audio.start (now, click, 0, 0.8f);
        audio.addBacking (0.0, 0.0);
        audio.addClicks (clicks);
        CHECK (run (1.2, 0.0) > 0.05f);
        // The audio keeps running on its own thread, as in the app, while Stop is pressed.
        std::atomic<bool> rendering { true };
        std::thread audioThread ([&] { while (rendering) { run (block / rate, 0.0); std::this_thread::sleep_for (std::chrono::milliseconds (5)); } });
        std::this_thread::sleep_for (std::chrono::milliseconds (50));
        const auto before = juce::Time::getMillisecondCounterHiRes();
        audio.stop();
        const auto took = juce::Time::getMillisecondCounterHiRes() - before;
        rendering = false;
        audioThread.join();
        CHECK (took < 100.0);                 // no waiting for the player
        CHECK (run (2.0, 0.15) < 1.0e-4f);    // click and backing both silent
        audio.clearBacking();
        wav.deleteFile();
    }

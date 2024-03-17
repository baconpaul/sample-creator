Nothing to see here.

Big swaths of work until there is though

- Format Support
    - Is the inst block correct? Endpoint check etc
    - other sample metadata?
- UI
    - Double buffer keyboard
    - Light skin doesn't suck (and is objeyed in step() in the Skin)
    - Scroll bar on the log would be nice eh
    - Buttons get glyphs not text
    - Path regino shows path and has a set button rather than just massive button
    - An actual design! Lets hope Steve bites :)
- Important Details
    - Any error handling at all in the RIFF writer
    - Try/Catch around all filesystem things
    - A way to display errors on screen in the log as red text
    - store the path used in the patch
- Engine / Rendering
    - "Drone Mode" - what is it and how would it work?. I like the idea of cut in
      half and add fades but controlling that is tricky
    - Loop Points - should we do something?
    - A "Latency Mode" for when resampling host
    - Optionally render polyphonically to save time?

Crashes

- Press test then press Start without stopping is a crash
- Occasional stopping while writing crashes like this. Propbably a clsoe order mismatch

```bash

Thread 10 Crashed:
0   libsystem_c.dylib             	       0x1879e57b4 flockfile + 28
1   libsystem_c.dylib             	       0x1879f5e04 fwrite + 84
2   plugin.dylib                  	       0x10e83cd8c baconpaul::samplecreator::riffwav::RIFFWavWriter::pushInterleavedBlock(float*, unsigned long) + 56 (RIFFWavWriter.hpp:90)
3   plugin.dylib                  	       0x10e835f94 baconpaul::samplecreator::SampleCreatorModule::renderThreadWriteBlock(int) + 144 (SampleCreatorModule.hpp:412)
4   plugin.dylib                  	       0x10e8348dc baconpaul::samplecreator::SampleCreatorModule::renderThreadProcess() + 1568 (SampleCreatorModule.hpp:357)
5   plugin.dylib                  	       0x10e8342b0 baconpaul::samplecreator::SampleCreatorModule::SampleCreatorModule()::'lambda'()::operator()() const + 28 (SampleCreatorModule.hpp:180)
6   plugin.dylib                  	       0x10e834260 decltype(std::declval<baconpaul::samplecreator::SampleCreatorModule::SampleCreatorModule()::'lambda'()>()()) std::__1::__invoke[abi:ue170006]<baconpaul::samplecreator::SampleCreatorModule::SampleCreatorModule()::'lambda'()>(baconpaul::samplecreator::SampleCreatorModule::SampleCreatorModule()::'lambda'()&&) + 24 (invoke.h:340)
7   plugin.dylib                  	       0x10e8341fc void std::__1::__thread_execute[abi:ue170006]<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct>>, baconpaul::samplecreator::SampleCreatorModule::SampleCreatorModule()::'lambda'()>(std::__1::tuple<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct>>, baconpaul::samplecreator::SampleCreatorModule::SampleCreatorModule()::'lambda'()>&, std::__1::__tuple_indices<>) + 28 (thread.h:227)
8   plugin.dylib                  	       0x10e833b5c void* std::__1::__thread_proxy[abi:ue170006]<std::__1::tuple<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct>>, baconpaul::samplecreator::SampleCreatorModule::SampleCreatorModule()::'lambda'()>>(void*) + 84 (thread.h:238)
9   libsystem_pthread.dylib       	       0x187b3d034 _pthread_start + 136
10  libsystem_pthread.dylib       	       0x187b37e3c thread_start + 8

```
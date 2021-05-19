Wrapper for Arduino's INO files (necessary for Platform IO). INO files are concatenated by the Arduino IDE's preprocessor, that's why the include statements come close to the behavior of the Arduino IDE.

The Arduino IDE's preprocessor does a couple of other things (such as automatic forward declarations of all functions). When using Platform IO, be careful of what C/C++ features you use and continuously check whether it is still compiling with the Arduino IDE.

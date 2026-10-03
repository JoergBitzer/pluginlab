/* Added at configure time in front of FST's fst.h (see CMakeLists.txt, "VST2"):
   - the FST "unknown" opcodes and speaker arrangements are not marked deprecated (JUCE's wrapper names them all)
   - the compiler treats the header like a system header: no warnings about FST's flexible array members */
#define FST_DONT_DEPRECATE_UNKNOWN 1
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC system_header
#endif

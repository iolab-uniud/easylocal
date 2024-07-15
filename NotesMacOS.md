# Compiling on MacOS with better c++23 support

Given that the AppleClang compiler (version 15 at the time of writing) is not yet fully equipped to support c++23 features, such as the "Explicit object parameter (deducing this)" for simplifying CRTP occurrences, we have a reliable workaround. This involves using llvm >= 18 or gcc >= 14 for code compilation. These tools, while not the default, are robust and effective, providing a seamless transition to c++23 features. Below are some instructions for getting a suitable llvm, which also supports Xcode.

## Installing `llvm`

Install `llvm` through `homebrew`

```sh
brew install llvm
```

This will create a homebrew cellar in `/opt/homebrew/Cellar/llvm/<version>`. The `CMakeFiles.txt` will check for a suitable compiler in that directory (under MacOS).

### Enabling `llvm` in Xcode

If you are interested in using Xcode (e.g., for supporting debugging), you should perform the following steps:

1. Install the *toolchain* into `~/Library/Developer/Toolchains`; replace `<version>` with the installed `llvm` version.
   ```sh
   mkdir ~/Library/Developer/Toolchains
   ln -s /opt/homebrew/Cellar/llvm/<version> ~/Library/Developer/Toolchains
   ```

   This will enable the Xcode toolchain. You can check it by opening Xcode and checking whether a Toolchains menu option appears now in the Xcode menu dropdown (the leftmost); select the new toolchain.

2. For generating an Xcode project that uses that toolchain, you should specify the `-T` flag to CMake as follows:

   ```sh
   cmake -G Xcode -T "org.llvm.<version>" ..
   ```

3. If you are customizing the `CMakeFiles.txt`, recall that you should disable the Xcode parameter `COMPILER_INDEX_STORE_ENABLE` to compile properly. To do it, you should add the following to your file:
   ```cmake
   set_target_properties(<your-target>
                         PROPERTIES
                         XCODE_ATTRIBUTE_COMPILER_INDEX_STORE_ENABLE NO)
   ```

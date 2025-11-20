# Coco Library

**Coco** is a comprehensive, header-only C++ library designed for precise time measurements, benchmarking, and visual profiling. It provides tools for logging execution times to the console, gathering statistical data, and generating JSON reports compatible with Chrome Tracing.

## Features

* **Header-Only:** Easy to integrate. Just include the file and you are ready to go.
* **Visual Profiling:** Generates JSON output compatible with `chrome://tracing` or [ui.perfetto.dev](https://ui.perfetto.dev/) to visualize function calls and performance bottlenecks.
* **RAII Timers:** Scoped timers that automatically measure and print execution time when a block ends.
* **Statistical Analysis:** Calculate average, variance, standard deviation, median, min, and max execution times.
* **Thread Safe:** profiling handles thread IDs correctly.
* **Modern C++:** Supports C++17 and utilizes C++20 Concepts if available.
* **Zero Overhead Option:** Profiling macros can be completely disabled for release builds via `COCO_NO_PROFILE`.

## Requirements

* C++17 or higher.

## Installation

Since Coco is a header-only library, you just need to include the header file in your project.

1.  Copy the header file (e.g., `coco.h`) to your project's include directory.
2.  Include it in your code:
    ```cpp
    #include "coco.h"
    ```

## Usage

### 1. Basic Console Timing (Scoped Timer)

The simplest way to measure how long a function or scope takes is using `COCO_SCOPE_TIMER`. It prints the result to `std::cout` automatically when the scope ends.

```cpp
void HeavyTask()
{
    // Starts the timer here.
    // Will print "HeavyTask Timer : X microseconds" when function exits.
    COCO_SCOPE_TIMER_NAMED("HeavyTask Timer");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
```

### 2. Visual Profiling (Chrome Tracing)

Coco allows you to instrument your code to see exactly what your program is doing over time.

**Step 1: Instrument your code**

```cpp
#include "coco.h"

void SubOperation()
{
    COCO_PROFILE_FUNCTION(); // Auto-names based on function signature
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

void MainOperation()
{
    COCO_PROFILE_SCOPE("Main Loop"); // Manual naming
    for(int i = 0; i < 5; i++)
        SubOperation();
}

int main()
{
    // Begin session: creates 'results.json'
    COCO_PROFILE_BEGIN_SESSION("Startup Benchmark", "results.json");

    MainOperation();

    // End session: closes the file and writes the footer
    COCO_PROFILE_END_SESSION();
}
```

**Step 2: View the results**
1.  Open Google Chrome.
2.  Type `chrome://tracing` in the address bar.
3.  Click **Load** and select the generated `results.json` file.
4.  You will see a visual timeline of your function calls.

### 3. Disabling Profiling

For release builds, you can disable all profiling overhead by defining `COCO_NO_PROFILE` before including the library or adding it to your preprocessor definitions.

```cpp
#define COCO_NO_PROFILE
#include "coco.h"

// Now, macros like COCO_PROFILE_FUNCTION() compile to nothing.
```

### 4. Statistical Benchmarking

You can run a function multiple times to get statistical data (Variance, Standard Deviation, Median, etc.).

```cpp
void MyAlgorithm(int a, int b) {
    // calculation...
}

int main() {
    // Run MyAlgorithm 100 times and log stats to 'stats.txt'
    coco::measure(100, "stats.txt", MyAlgorithm, 10, 20);
}
```

## API Summary

| Macro | Description |
| :--- | :--- |
| `COCO_SCOPE_TIMER()` | Creates a timer that prints duration to console upon destruction (unnamed). |
| `COCO_SCOPE_TIMER_NAMED(name)` | Creates a named timer that prints to console upon destruction. |
| `COCO_PROFILE_BEGIN_SESSION(name, path)` | Starts a profiling session and opens the JSON file. |
| `COCO_PROFILE_END_SESSION()` | Ends the session and flushes data to the file. |
| `COCO_PROFILE_FUNCTION()` | Profiles the current function (uses function signature as name). |
| `COCO_PROFILE_SCOPE(name)` | Profiles the current scope with a custom name. |

## License

This library is released under the **Apache License 2.0**.
See the `LICENSE` file for more details.

## Author

Original library created by **Tynes0**.
* GitHub: [https://github.com/tynes0](https://github.com/tynes0)
* Email: cihanbilgihan@gmail.com

# Contributing

ActiveRecord for C++ is a spare-time project with one maintainer. Issues and pull requests are welcome, but reviews may take a while.

## Before You Start

- Bug reports: include the adapter, compiler and platform, and a minimal example if you can.
- Larger changes: open an issue first, so we can agree on the approach before you write the code.

## Making Changes

- Follow [AGENTS.md](AGENTS.md): it holds the project's design criteria and coding standards, for human contributors and AI assistants alike.
- Format every C/C++ file you change with `clang-format` (repository `.clang-format`, LLVM 19).
- Keep the build warning-free: warnings and clang-tidy findings are errors.
- Add or update tests for the behavior you change, and run them: `ctest --test-dir build -C Release --output-on-failure`.
- Run the [hardening checks](docs/hardening.md) that apply to your change, for example the sanitizer build for low-level code or the adapter runs for adapter changes.

## Supported Platforms

Changes must build and pass the tests with the tested compilers: MSVC (Visual Studio 2022) on Windows, and GCC 14+ and Clang 18+ on Linux. See [docs/compilers.md](docs/compilers.md).

## License

By contributing, you agree that your contributions are licensed under the [MIT License](LICENSE).

# wen

[![Stars](https://img.shields.io/github/stars/rinelu/wen?style=social)](https://github.com/rinelu/wen/stargazers)
![Tests](https://github.com/rinelu/wen/actions/workflows/wen.yaml/badge.svg)

`wen` is a **low-level networking library written in C**, designed with explicit lifetimes and user-managed I/O. It provides a minimal, safe API for working with byte streams and building protocol codecs without hidden state or allocations.

## Features

- **Low-level networking primitives** - focused on control and predictability  
-  **Explicit lifetimes** - no hidden ownership, everything is managed by the caller  
- **User-managed I/O** - your code controls buffers and reads/writes  
- **Protocol codec helpers** - utilities to build higher-level protocols  

## License

This project is licensed under the terms shown in the `LICENSE` file.

## Contributing

Contributions are welcome! If you find bugs, want to add examples, improve documentation, or enhance the API open an issue or a pull request.

## Support

If you find `wen` useful, consider starring the repository on GitHub.

# Getting Help with Yalu

Thank you for exploring the **Yalu Programming Language**! Here is how to find documentation, ask questions, report bugs, and request features.

---

## 1. Documentation & References

Before opening an issue, check the official documentation files located in this repository:

- **[README.md](README.md)**: Project overview, installation, quick-start guide, and feature walkthrough.
- **[Language Specification](docs/spec.md)**: Formal EBNF grammar, lexical rules, scoping semantics, and execution model.
- **[Syntax Reference](docs/syntax.md)**: Detailed syntax guide with examples for all constructs.
- **[Architecture Guide](docs/architecture.md)**: Deep dive into the custom C11 lexer, parser, AST, value representation, and interpreter pipeline.
- **[Examples Directory](examples/)**: Runnable sample programs illustrating core features (banking, calculator, Fibonacci, module imports, etc.).

---

## 2. Reporting Issues & Bugs

If you find a bug, interpreter crash, unexpected runtime behavior, or syntax parsing failure:

1. **Search Existing Issues**: Check if the issue has already been reported.
2. **Submit a Minimal Reproducible Example**: Include a concise `.yl` source snippet demonstrating the problem.
3. **Include System Details**: Provide your operating system version, compiler toolchain, and the output of:
   ```cmd
   yalu.exe --version
   ```

---

## 3. Requesting Features & Enhancements

Yalu evolves through deliberate, design-conscious decisions. When suggesting a feature:
- Explain the motivation and use cases.
- Provide example `.yl` syntax conforming to Yalu's established design language (e.g. delimiters like `!!`, `::`, `<<""`, `>>`, `place`).
- Discuss potential trade-offs and backward compatibility impacts.


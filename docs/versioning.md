# Versioning and Release Strategy

## Version Scheme

pcode-editor uses **Semantic Versioning (SemVer)** with the format:

```
vMAJOR.MINOR.PATCH
```

| Component | Description | When to Increment |
|------------|-------------|-------------------|
| **MAJOR** | Breaking changes | Incompatible API/feature changes |
| **MINOR** | New features | Backward-compatible new features |
| **PATCH** | Bug fixes | Backward-compatible bug fixes |

### Current Version

**v0.2.28** (2026-04-17)

- 0.x series indicates pre-release (experimental)
- Will become v1.0.0 when feature-complete

---

## Git Tags

### Purpose

1. **Mark releases** - Clearly identify release commits
2. **Historical reference** - Easy rollback and comparison
3. **CI/CD integration** - Build pipelines can trigger on tags
4. **User reference** - Users can checkout specific versions

### Why Annotated Tags?

- Store author, date, and message
- More informative than lightweight tags
- Preferred for releases

### Why Not Tag Every Commit?

- Creates noise in tag history
- Most commits are intermediate/broken
- Tags should represent meaningful states

---

## Related Documents

- [README.md](../README.md)
- [User Guide](userguide.md)
- [Developer Guide](developer.md)
- [FAQ](faq.md)
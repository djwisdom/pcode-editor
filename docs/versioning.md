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

**v0.2.38** (2026-04-18)

- 0.x series indicates pre-release (experimental)
- Will become v1.0.0 when feature-complete

---

## Version Bump Rule

**Every commit must bump the version number** in:
1. `VERSION` file (contains just the version string)
2. Status bar (displays version)
3. Commit message format: `v{version} ({git_hash})`

### When to Increment

| Change Type | Increment |
|------------|----------|
| Bug fix | PATCH (+1) |
| New feature | MINOR (+1) |
| Breaking change | MAJOR (+1) |

**Example:** v0.2.37 → v0.2.38

### Overflow Rule

When PATCH reaches 99, reset to 0 and increment MINOR:
- `v0.2.99` → `v0.3.0`
- `v0.99.99` → `v1.0.0`

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
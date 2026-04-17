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

1. **Mark releases** — Clearly identify release commits
2. **Historical reference** — Easy rollback and comparison
3. **CI/CD integration** — Build pipelines can trigger on tags
4. **User reference** — Users can checkout specific versions

### Tag Format

| Type | Format | Example |
|------|-------|---------|
| Release | `v*.*.*` | `v0.2.28` |
| Beta | `v*.*.*-beta*` | `v0.3.0-beta1` |
| Release Candidate | `v*.*.*-rc*` | `v0.3.0-rc1` |

### Creating Tags

```bash
# Annotated tag (recommended for releases)
git tag -a v0.2.28 -m "Release v0.2.28 - Description"

# Lightweight tag
git tag v0.2.28
```

### Managing Tags

```bash
# List all tags
git tag -l

# List tags with messages
git tag -l -n

# Push tag to remote
git push origin v0.2.28

# Push all tags
git push --tags

# Delete local tag
git tag -d v0.2.28

# Delete remote tag
git push origin :refs/tags/v0.2.28
```

---

## Release Process

### Before Releasing

1. **Update VERSION file** with new version number
2. **Update README.md** with new version in header
3. **Update documentation** if needed
4. **Run tests** and verify build

### Creating a Release

```bash
# 1. Ensure on main branch and up to date
git checkout main
git pull

# 2. Create the release tag
git tag -a v0.2.28 -m "Release v0.2.28 - Description of changes"

# 3. Push tag to remote
git push origin v0.2.28
```

### After Release

1. Update VERSION to next development version (e.g., v0.2.29-dev)
2. Push the version change

---

## Version File

The `VERSION` file contains only the version string:

```
0.2.28
```

This file is used by:
- Build scripts to embed version in executable
- Documentation for version reference
- CI/CD for release detection

### Sync Version Script

Use the provided scripts to sync version:

```bash
# PowerShell (Windows)
.\scripts\sync-version.ps1 0.2.29

# Bash (Linux/macOS/BSD)
./scripts/sync-version.sh 0.2.29
```

---

## Release Checklist

- [ ] All tests pass
- [ ] Build succeeds on all platforms
- [ ] VERSION file updated
- [ ] README.md header updated
- [ ] Changelog entry added (if applicable)
- [ ] Documentation updated
- [ ] Git tag created with version
- [ ] Tag pushed to remote

---

## Rationale

### Why Use Tags?

1. **Clarity** — Tags make it obvious which commit is a release
2. **Navigation** — Easy to find any release version
3. **CI/CD** — Can trigger builds/deployments on tags
4. **Collaboration** — Team members can reference exact versions

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
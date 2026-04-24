# Docker Build Optimization - Expected Improvements

## Current Performance

| Metric | Current |
|--------|---------|
| Full build | 98 minutes |
| vcpkg clone/bootstrap | 10-15 min |
| Dependency install | 20-30 min |
| Application build | 30-40 min |
| Cache hit rate | Low (GHA cache only) |

## Optimizations Implemented

### 1. Dockerfile Multi-Stage Improvements

**Stage 1: vcpkg-base**
- Separate layer for vcpkg and dependencies
- Cache hit when `vcpkg.json` unchanged
- Source code changes don't invalidate vcpkg layer

**Stage 2: build**
- Only copy source files after dependencies installed
- Maximum layer cache utilization

**Stage 3: runtime**
- Minimal image (no build tools)
- Non-root user for security

### 2. Workflow Caching Strategy

**Dual cache sources:**
1. **Registry cache** (`ghcr.io/../mosqueeze-server:cache`)
   - Best for large layers (vcpkg, dependencies)
   - Survives workflow cleanup
   - Shared across PRs

2. **GHA cache** (Actions cache)
   - Fast for incremental changes
   - Limited to 10GB

**Cache flow:**
```
1. Pull registry cache (cached layers)
2. Pull GHA cache (fast incremental)
3. Build with cache-from both sources
4. Push to registry cache (for next build)
5. Push to GHA cache
```

### 3. Build Order Optimization

```
# Before (invalidates cache on any file change)
COPY . .
RUN cmake && build

# After (separate layers)
COPY vcpkg.json vcpkg-configuration.json* ./  # Only deps manifest
RUN cmake -B build-deps                        # Install deps (cached)
COPY src/ ./src/                               # Source (separate)
COPY include/ ./include/                       # Headers (separate)
RUN cmake --build                               # Build app
```

### 4. Additional Improvements

- **SBOM & Provenance**: Security attestation
- **No-cache option**: Force rebuild via `workflow_dispatch`
- **Non-root user**: Security best practice
- **BuildKit debug**: Better troubleshooting

## Expected Performance

| Scenario | Before | After | Improvement |
|----------|--------|-------|-------------|
| Full rebuild | 98 min | 60-70 min | ~30-40% |
| vcpkg.json unchanged | 98 min | 20-30 min | ~70-80% |
| Code-only change (deps cached) | 98 min | 10-15 min | ~85% |
| Same commit rebuild | 98 min | 2-5 min | ~95% |

## File Changes

```
src/mosqueeze-server/Dockerfile        → Multi-stage with cache optimization
.github/workflows/docker-server.yml    → Dual cache, registry cache
```

## Usage

### Normal push to main
```bash
git push origin main
# → Uses cached layers if vcpkg.json unchanged
# → ~20-30 min if deps cached
```

### Force full rebuild
```
# Use workflow_dispatch with no_cache=true
# → Rebuilds all layers
# → ~60-70 min
```

### Multi-platform build
```
# Use workflow_dispatch with platforms=linux/amd64,linux/arm64
# → Slower but produces both images
# → ~90-120 min (amd64 native + arm64 QEMU)
```
# Corpus Selection

**Summary**: Criteri per la selezione dei file di test nel corpus.

**Last updated**: 2026-04-22

---

## Principles

### 1. Rappresentatività

I file del corpus devono rappresentare casi d'uso reali:

| Categoria | Esempi | Target Use Case |
|-----------|--------|-----------------|
| Codice sorgente | Linux kernel subset, React source | Backup repository |
| JSON/XML | Twitter sample, OpenAPI specs | API responses |
| Logs | Server access logs, app logs | Log archiving |
| Binary | Executables, libraries | Software backup |
| Database | SQLite dumps | Database backup |

### 2. Dimension Variety

```
Range di dimensioni:
- Small:   10 KB - 100 KB    (config files, scripts)
- Medium:  100 KB - 10 MB    (source files, JSON)
- Large:   10 MB - 100 MB    (logs, database slices)
- Huge:    100 MB - 1 GB     (full database, archives)
```

### 3. License/Openness

- ✅ Public domain
- ✅ MIT/Apache licensed
- ✅ Self-generated (dummy data)
- ❌ Proprietary data
- ❌ Copyrighted content

---

## Current Corpus

### text/code/

| File | Size | Type | Source |
|------|------|------|--------|
| `large.cpp` | 2.1 MB | C++ | Generated pattern |
| `bundle.js` | 5.8 MB | JavaScript | Minified webpack bundle |
| `react_source/` | 12 MB | Mixed | React repo subset |

### text/json/

| File | Size | Type | Source |
|------|------|------|--------|
| `large_dataset.json` | 5.2 MB | JSON | Generated nested structure |
| `api_response.json` | 800 KB | JSON | Typical REST response |

### text/logs/

| File | Size | Type | Source |
|------|------|------|--------|
| `access.log` | 15 MB | Apache format | anonymized |
| `app.log` | 8 MB | JSON lines | Generated |

### binary/exec/

| File | Size | Type | Source |
|------|------|------|--------|
| `test_binary.exe` | 8 MB | PE | Compiled test program |
| `library.so` | 4 MB | ELF | Compiled library |

---

## Generating Synthetic Data

### JSON Generator

```python
import json
import random

def generate_json(depth=4, items_per_level=100):
    if depth == 0:
        return random_string(50)
    
    result = {}
    for i in range(items_per_level):
        key = f"item_{i}"
        if random.random() > 0.5:
            result[key] = generate_json(depth-1, items_per_level//2)
        else:
            result[key] = random_string(random.randint(20, 200))
    
    return result
```

### Log Generator

```python
from datetime import datetime, timedelta
import random

def generate_log_lines(count=100000):
    levels = ["INFO", "WARN", "ERROR", "DEBUG"]
    services = ["auth", "api", "worker", "scheduler"]
    
    base_time = datetime.now() - timedelta(days=30)
    lines = []
    
    for i in range(count):
        timestamp = (base_time + timedelta(seconds=i)).isoformat()
        level = random.choice(levels)
        service = random.choice(services)
        message = generate_message()
        
        lines.append(f"{timestamp} [{service}] {level}: {message}")
    
    return "\n".join(lines)
```

---

## Future Expansion

### Planned Additions

1. **ZPAQ comparison files** — Quando zpaq engine sarà implementato
2. **Image corpus** — PNG, BMP tests
3. **Archive tests** — TAR, ZIP extraction benchmark
4. **Edge cases** — Empty files, single byte, repeated patterns

### Size Goals

```
Target dimensioni totali per categoria:
- text/code:     50 MB
- text/json:     30 MB
- text/logs:     100 MB
- binary/:       150 MB
- image/:        50 MB (future)
─────────────────────────
Total:           ~400 MB
```

---

## Downloading/Public

### Pre-existing Corpora

1. **Silesia Corpus** — Classic compression test files
2. **Canterbury Corpus** — Standard compression benchmark
3. **Calgary Corpus** — Historical compression test set

### Integration

```bash
# scripts/download_corpus.sh
curl -O https://corpus.data/silesia.zip
unzip silesia.zip -d corpus/external/silesia/
```

---

## Legal/Ethical

### Data Anonymization

Per log con user data:
```python
def anonymize_log(line):
    # Remove IPs
    line = re.sub(r'\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}', 'XXX.XXX.XXX.XXX', line)
    
    # Remove emails
    line = re.sub(r'[\w\.-]+@[\w\.-]+\.\w+', 'user@example.com', line)
    
    # Remove tokens
    line = re.sub(r'token=[\w-]+', 'token=REDACTED', line)
    
    return line
```

---

## Related Pages

- [[methodology]] — Come eseguire benchmark
- [[results/index]] — Risultati storici
- [[graphs/ratio-by-algorithm]] — Grafici
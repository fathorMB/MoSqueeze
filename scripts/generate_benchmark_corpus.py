#!/usr/bin/env python3
"""
generate_benchmark_corpus.py

Generate synthetic benchmark files for MoSqueeze extended benchmarks.
Creates diverse test files across all categories.
"""

import json
import os
import random
import hashlib
import struct
import math
from pathlib import Path
from typing import List, Dict, Any

def calculate_entropy(data: bytes) -> float:
    """Calculate Shannon entropy in bits per byte."""
    if not data:
        return 0.0
    counts = {}
    for b in data:
        counts[b] = counts.get(b, 0) + 1
    total = len(data)
    entropy = 0.0
    for count in counts.values():
        p = count / total
        entropy -= p * math.log2(p)
    return entropy


# =============================================================================
# JSON Generators
# =============================================================================

def generate_json_api_response(size_kb: int = 10) -> Dict[str, Any]:
    """Generate a realistic REST API response JSON."""
    items = []
    num_items = size_kb * 5  # Rough estimate
    
    for i in range(num_items):
        items.append({
            "id": f"item_{i:06d}",
            "name": f"Product {i}",
            "description": f"This is a detailed description for product {i}. " * random.randint(1, 5),
            "price": round(random.uniform(9.99, 999.99), 2),
            "quantity": random.randint(0, 1000),
            "category": random.choice(["electronics", "clothing", "home", "garden", "sports"]),
            "tags": [f"tag_{j}" for j in range(random.randint(1, 10))],
            "metadata": {
                "created": f"2024-{random.randint(1,12):02d}-{random.randint(1,28):02d}T{random.randint(0,23):02d}:{random.randint(0,59):02d}:00Z",
                "updated": f"2024-{random.randint(1,12):02d}-{random.randint(1,28):02d}T{random.randint(0,23):02d}:{random.randint(0,59):02d}:00Z",
                "version": f"v{random.randint(1,5)}.{random.randint(0,99)}.{random.randint(0,99)}",
                "author": random.choice(["Alice", "Bob", "Charlie", "Diana", "Eve"]),
                "approved": random.choice([True, False])
            },
            "variants": [
                {
                    "sku": f"SKU-{i}-{j}",
                    "color": random.choice(["red", "blue", "green", "black", "white"]),
                    "size": random.choice(["XS", "S", "M", "L", "XL"]),
                    "stock": random.randint(0, 500)
                }
                for j in range(random.randint(1, 5))
            ]
        })
    
    return {
        "success": True,
        "timestamp": "2024-01-15T10:30:00Z",
        "pagination": {
            "page": 1,
            "per_page": num_items,
            "total": num_items * 10,
            "total_pages": 10
        },
        "data": items,
        "meta": {
            "api_version": "3.2.1",
            "server": "api-" + random.choice(["west", "east", "central"]) + ".example.com",
            "response_time_ms": random.randint(50, 500)
        }
    }


def generate_json_config(size_kb: int = 5) -> Dict[str, Any]:
    """Generate a large configuration JSON."""
    config = {
        "version": "1.0.0",
        "app_name": "MyApplication",
        "environment": random.choice(["development", "staging", "production"]),
        "settings": {},
        "features": {},
        "endpoints": [],
        "database": {
            "host": "localhost",
            "port": 5432,
            "name": "mydb",
            "ssl": True,
            "pool_size": 20
        }
    }
    
    # Generate many settings
    for i in range(size_kb * 10):
        config["settings"][f"setting_{i}"] = {
            "value": f"value_{i}_{random.randint(1000, 9999)}",
            "type": random.choice(["string", "number", "boolean", "object"]),
            "description": f"Description for setting {i}. " * random.randint(1, 3),
            "default": random.choice([True, False, None, "default_value"]),
            "options": [f"option_{j}" for j in range(random.randint(2, 10))]
        }
    
    # Generate features
    for i in range(size_kb * 2):
        config["features"][f"feature_{i}"] = {
            "enabled": random.choice([True, False]),
            "percentage": random.randint(0, 100),
            "config": {
                "param_a": random.randint(1, 100),
                "param_b": random.choice(["a", "b", "c"]),
                "param_c": [random.randint(1, 1000) for _ in range(random.randint(3, 10))]
            }
        }
    
    # Generate endpoints
    for i in range(size_kb * 3):
        config["endpoints"].append({
            "path": f"/api/v1/resource_{i}",
            "method": random.choice(["GET", "POST", "PUT", "DELETE", "PATCH"]),
            "auth": random.choice([True, False]),
            "rate_limit": random.randint(10, 1000),
            "cache_ttl": random.randint(0, 3600),
            "middlewares": random.sample(["auth", "logging", "metrics", "cors", "ratelimit"], 
                                          k=random.randint(1, 5))
        })
    
    return config


def generate_json_dataset(num_records: int = 1000) -> List[Dict[str, Any]]:
    """Generate a large dataset JSON."""
    records = []
    for i in range(num_records):
        records.append({
            "id": i,
            "timestamp": f"2024-{random.randint(1,12):02d}-{random.randint(1,28):02d}T{random.randint(0,23):02d}:{random.randint(0,59):02d}:{random.randint(0,59):02d}Z",
            "user_id": f"user_{random.randint(1, 10000)}",
            "event": random.choice(["click", "view", "purchase", "signup", "logout"]),
            "properties": {
                "page": f"/page/{random.randint(1, 100)}",
                "referrer": random.choice(["google", "facebook", "direct", "email", "other"]),
                "duration": random.randint(1, 600),
                "device": random.choice(["desktop", "mobile", "tablet"]),
                "browser": random.choice(["chrome", "firefox", "safari", "edge"]),
                "country": random.choice(["US", "UK", "DE", "FR", "JP", "CN", "BR", "IN"])
            },
            "metadata": {
                "session_id": hashlib.md5(str(random.random()).encode()).hexdigest()[:16],
                "ip": f"{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}",
                "user_agent": random.choice([
                    "Mozilla/5.0 (Windows NT 10.0; Win64; x64)",
                    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)",
                    "Mozilla/5.0 (X11; Linux x86_64)",
                    "Mozilla/5.0 (iPhone; CPU iPhone OS 14_0)"
                ])
            }
        })
    return records


# =============================================================================
# XML Generators
# =============================================================================

def generate_xml_rss_feed(num_items: int = 100) -> str:
    """Generate an RSS feed XML."""
    items_xml = []
    for i in range(num_items):
        items_xml.append(f"""
    <item>
      <title>Article {i}: {random.choice(['Technology', 'Science', 'Business', 'Sports'])} News</title>
      <link>https://example.com/article/{i}</link>
      <description>{"This is a description of article " + str(i) + ". " * random.randint(3, 10)}</description>
      <author>{random.choice(['Alice', 'Bob', 'Charlie', 'Diana'])}</author>
      <pubDate>2024-{random.randint(1,12):02d}-{random.randint(1,28):02d}</pubDate>
      <category>{random.choice(['tech', 'science', 'business', 'sports'])}</category>
      <guid isPermaLink="true">https://example.com/article/{i}</guid>
    </item>""")
    
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom">
  <channel>
    <title>Example News Feed</title>
    <link>https://example.com</link>
    <description>Latest news and updates</description>
    <language>en-us</language>
    <lastBuildDate>2024-01-15T10:00:00Z</lastBuildDate>
    <atom:link href="https://example.com/rss.xml" rel="self" type="application/rss+xml"/>
{''.join(items_xml)}
  </channel>
</rss>"""


def generate_svg_graphic(size_kb: int = 10) -> str:
    """Generate an SVG with many elements."""
    shapes = []
    width = 1000
    height = 1000
    
    for i in range(size_kb * 50):
        shape_type = random.choice(["rect", "circle", "line", "polygon", "path"])
        
        if shape_type == "rect":
            shapes.append(f'<rect x="{random.randint(0, width-50)}" y="{random.randint(0, height-50)}" '
                        f'width="{random.randint(10, 100)}" height="{random.randint(10, 100)}" '
                        f'fill="rgb({random.randint(0,255)},{random.randint(0,255)},{random.randint(0,255)})" '
                        f'opacity="{random.uniform(0.3, 1.0):.2f}"/>')
        elif shape_type == "circle":
            shapes.append(f'<circle cx="{random.randint(0, width)}" cy="{random.randint(0, height)}" '
                        f'r="{random.randint(5, 50)}" '
                        f'fill="rgb({random.randint(0,255)},{random.randint(0,255)},{random.randint(0,255)})" '
                        f'opacity="{random.uniform(0.3, 1.0):.2f}"/>')
        elif shape_type == "line":
            shapes.append(f'<line x1="{random.randint(0, width)}" y1="{random.randint(0, height)}" '
                        f'x2="{random.randint(0, width)}" y2="{random.randint(0, height)}" '
                        f'stroke="rgb({random.randint(0,255)},{random.randint(0,255)},{random.randint(0,255)})" '
                        f'stroke-width="{random.randint(1, 5)}"/>')
        elif shape_type == "polygon":
            points = " ".join([f"{random.randint(0, width)},{random.randint(0, height)}" 
                              for _ in range(random.randint(3, 8))])
            shapes.append(f'<polygon points="{points}" '
                        f'fill="rgb({random.randint(0,255)},{random.randint(0,255)},{random.randint(0,255)})" '
                        f'opacity="{random.uniform(0.3, 1.0):.2f}"/>')
        else:  # path
            d = f"M {random.randint(0, width)} {random.randint(0, height)}"
            for _ in range(random.randint(2, 5)):
                d += f" L {random.randint(0, width)} {random.randint(0, height)}"
            shapes.append(f'<path d="{d}" '
                        f'stroke="rgb({random.randint(0,255)},{random.randint(0,255)},{random.randint(0,255)})" '
                        f'fill="none" stroke-width="{random.randint(1, 3)}"/>')
    
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
  <title>Generated SVG {size_kb}KB</title>
  <desc>Complex SVG with many shapes for compression testing</desc>
{''.join(shapes)}
</svg>"""


# =============================================================================
# Source Code Generators
# =============================================================================

def generate_javascript_code(lines: int = 1000, minified: bool = False) -> str:
    """Generate JavaScript code."""
    code_parts = []
    
    # Classes
    for i in range(lines // 100):
        code_parts.append(f"""
class MyClass{i} {{
  constructor(name, value) {{
    this.name = name;
    this.value = value;
    this.createdAt = new Date();
  }}
  
  process(callback) {{
    return callback(this.name, this.value);
  }}
  
  async fetchData(url) {{
    const response = await fetch(url);
    return response.json();
  }}
}}
""")
    
    # Functions
    for i in range(lines // 50):
        code_parts.append(f"""
function processData{i}(data) {{
  const result = data.map(item => ({{
    ...item,
    processed: true,
    timestamp: Date.now(),
    id: '{i}_' + item.id
  }}));
  return result.filter(item => item.active);
}}

const compute{i} = (arr) => arr.reduce((sum, val) => sum + val, 0);
""")
    
    # Variables
    for i in range(lines // 20):
        code_parts.append(f"""
const CONFIG_{i} = {{
  apiUrl: 'https://api.example.com/v{i}',
  timeout: {random.randint(1000, 10000)},
  retries: {random.randint(1, 5)},
  headers: {{
    'Content-Type': 'application/json',
    'Authorization': 'Bearer token_{hashlib.md5(str(i).encode()).hexdigest()[:8]}'
  }}
}};
""")
    
    # Comments (if not minified)
    if not minified:
        for i in range(lines // 200):
            code_parts.append(f"""
/**
 * This is a documentation comment for function {i}.
 * It explains what the function does and why it exists.
 * @param {{Object}} data - The input data to process
 * @returns {{Object}} The processed result
 * @throws {{Error}} If data is invalid
 */
""")
    
    code = "".join(code_parts)
    
    if minified:
        # Simple minification (remove whitespace and comments)
        import re
        code = re.sub(r'/\*\*?\*/', '', code)  # Remove comments
        code = re.sub(r'//.*$', '', code, flags=re.MULTILINE)  # Remove // comments
        code = re.sub(r'\s+', ' ', code)  # Collapse whitespace
        code = re.sub(r'\s?([{}();,:])\s?', r'\1', code)  # Remove space around tokens
        return code
    
    return code


def generate_python_code(lines: int = 1000) -> str:
    """Generate Python code."""
    code_parts = []
    
    # Classes
    for i in range(lines // 100):
        code_parts.append(f'''
class DataProcessor{i}:
    """Process data with various transformations."""
    
    def __init__(self, name: str, config: dict):
        self.name = name
        self.config = config
        self._cache = {{}}
    
    def process(self, data: list) -> list:
        """Process a list of items."""
        results = []
        for item in data:
            processed = self._transform(item)
            results.append(processed)
        return results
    
    def _transform(self, item: dict) -> dict:
        """Transform a single item."""
        return {{
            **item,
            "processed": True,
            "processor": self.name,
            "version": "{random.randint(1, 5)}.{random.randint(0, 99)}.{random.randint(0, 99)}"
        }}
''')
    
    # Functions
    for i in range(lines // 50):
        code_parts.append(f'''
def calculate_{i}(values: list, scale: float = 1.0) -> float:
    """Calculate weighted sum of values."""
    total = 0.0
    for j, val in enumerate(values):
        weight = 1.0 / (j + 1)
        total += val * weight
    return total * scale

async def fetch_data_{i}(url: str, timeout: int = 30) -> dict:
    """Fetch data from URL asynchronously."""
    import aiohttp
    async with aiohttp.ClientSession() as session:
        async with session.get(url, timeout=timeout) as response:
            return await response.json()
''')
    
    # Constants
    for i in range(lines // 30):
        code_parts.append(f'''
CONFIG_{i} = {{
    "url": "https://api.example.com/endpoint/{i}",
    "timeout": {random.randint(1000, 10000)},
    "retries": {random.randint(1, 5)},
    "enabled": {random.choice([True, False])},
    "tags": ["production", "api", "v{i}"],
}}
''')
    
    return "".join(code_parts)


def generate_cpp_code(lines: int = 1000) -> str:
    """Generate C++ code."""
    code_parts = []
    
    # Includes
    includes = "\n".join([
        "#include <vector>",
        "#include <string>",
        "#include <memory>",
        "#include <algorithm>",
        "#include <functional>",
        "#include <unordered_map>",
        "#include <iostream>",
    ])
    code_parts.append(includes + "\n\n")
    
    # Namespace
    code_parts.append("namespace Mosqueeze {\n\n")
    
    # Classes
    for i in range(lines // 150):
        code_parts.append(f'''
class Processor{i} {{
public:
    Processor{i}(const std::string& name, int timeout = {random.randint(1000, 10000)})
        : name_(name), timeout_(timeout) {{}}
    
    virtual ~Processor{i}() = default;
    
    std::vector<uint8_t> process(const std::vector<uint8_t>& data) {{
        std::vector<uint8_t> result;
        result.reserve(data.size());
        
        for (const auto& byte : data) {{
            auto transformed = transform(byte);
            result.push_back(transformed);
        }}
        
        return result;
    }}
    
private:
    std::string name_;
    int timeout_{random.randint(1, 5)};
    
    uint8_t transform(uint8_t input) {{
        return static_cast<uint8_t>((input * {random.randint(2, 7)}) % 256);
    }}
}};
''')
    
    # Functions
    for i in range(lines // 80):
        code_parts.append(f'''
std::vector<std::string> split_{i}(const std::string& input, char delimiter) {{
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = input.find(delimiter);
    
    while (end != std::string::npos) {{
        tokens.push_back(input.substr(start, end - start));
        start = end + 1;
        end = input.find(delimiter, start);
    }}
    tokens.push_back(input.substr(start));
    
    return tokens;
}}
''')
    
    # Close namespace
    code_parts.append("\n} // namespace Mosqueeze\n")
    
    return "".join(code_parts)


# =============================================================================
# CSV Generator
# =============================================================================

def generate_csv(rows: int = 1000) -> str:
    """Generate CSV data."""
    lines = ["id,timestamp,user_id,event,page,referrer,duration,device,browser,country,amount"]
    
    for i in range(rows):
        lines.append(",".join([
            str(i),
            f"2024-{random.randint(1,12):02d}-{random.randint(1,28):02d}T{random.randint(0,23):02d}:{random.randint(0,59):02d}:{random.randint(0,59):02d}Z",
            f"user_{random.randint(1, 10000)}",
            random.choice(["click", "view", "purchase", "signup", "logout"]),
            f"/page/{random.randint(1, 100)}",
            random.choice(["google", "facebook", "direct", "email", "other"]),
            str(random.randint(1, 600)),
            random.choice(["desktop", "mobile", "tablet"]),
            random.choice(["chrome", "firefox", "safari", "edge"]),
            random.choice(["US", "UK", "DE", "FR", "JP", "CN", "BR", "IN"]),
            str(round(random.uniform(9.99, 999.99), 2))
        ]))
    
    return "\n".join(lines)


# =============================================================================
# Binary Generators
# =============================================================================

def generate_random_bytes(size_kb: int = 100) -> bytes:
    """Generate high entropy random data."""
    return bytes(random.randint(0, 255) for _ in range(size_kb * 1024))


def generate_repeated_pattern(pattern_size: int = 64, repeat_count: int = 1000) -> bytes:
    """Generate data with repeated patterns."""
    pattern = bytes(random.randint(0, 255) for _ in range(pattern_size))
    return pattern * repeat_count


def generate_low_entropy(size_kb: int = 100) -> bytes:
    """Generate low entropy data (good compression potential)."""
    data = bytearray()
    for _ in range(size_kb * 1024):
        # Only use 16 possible values
        data.append(random.randint(0, 15) * 17)
    return bytes(data)


# =============================================================================
# Main Generator
# =============================================================================

def generate_corpus(base_dir: Path):
    """Generate entire benchmark corpus."""
    
    print("Generating benchmark corpus...")
    
    # JSON files
    print("  Generating JSON files...")
    for size_kb in [1, 5, 10, 50, 100]:
        # API responses
        json_path = base_dir / "documents" / "json" / "api-responses" / f"api_response_{size_kb}kb.json"
        json_data = generate_json_api_response(size_kb)
        json_path.write_text(json.dumps(json_data, indent=2))
        print(f"    Created {json_path.name} ({json_path.stat().st_size // 1024}KB)")
        
        # Configs
        if size_kb <= 20:
            config_path = base_dir / "documents" / "json" / "configs" / f"config_{size_kb}kb.json"
            config_data = generate_json_config(size_kb)
            config_path.write_text(json.dumps(config_data, indent=2))
            
        # Datasets
        if size_kb >= 10:
            dataset_path = base_dir / "documents" / "json" / "datasets" / f"dataset_{size_kb}kb.json"
            num_records = size_kb * 10
            dataset_data = generate_json_dataset(num_records)
            dataset_path.write_text(json.dumps(dataset_data))
    
    # JSON minified
    print("  Generating minified JSON...")
    for size_kb in [5, 10, 50]:
        api_data = generate_json_api_response(size_kb)
        minified = json.dumps(api_data, separators=(',', ':'))
        min_path = base_dir / "documents" / "json" / "minified" / f"api_minified_{size_kb}kb.json"
        min_path.write_text(minified)
    
    # XML files
    print("  Generating XML files...")
    for num_items in [100, 500, 1000]:
        rss_path = base_dir / "documents" / "xml" / "rss-feeds" / f"rss_{num_items}_items.xml"
        rss_content = generate_xml_rss_feed(num_items)
        rss_path.write_text(rss_content)
        print(f"    Created {rss_path.name} ({rss_path.stat().st_size // 1024}KB)")
    
    # SVG
    for size_kb in [10, 50, 100]:
        svg_path = base_dir / "documents" / "xml" / "svg-graphics" / f"graphic_{size_kb}kb.svg"
        svg_content = generate_svg_graphic(size_kb)
        svg_path.write_text(svg_content)
    
    # Source code
    print("  Generating source code...")
    for lines in [500, 1000, 5000]:
        # JavaScript
        js_path = base_dir / "source-code" / "javascript" / "unminified" / f"code_{lines}_lines.js"
        js_path.write_text(generate_javascript_code(lines, minified=False))
        
        js_min_path = base_dir / "source-code" / "javascript" / "minified" / f"code_{lines}_lines.min.js"
        js_min_path.write_text(generate_javascript_code(lines, minified=True))
        
        # Python
        py_path = base_dir / "source-code" / "python" / "scripts" / f"script_{lines}_lines.py"
        py_path.write_text(generate_python_code(lines))
        
        # C++
        cpp_path = base_dir / "source-code" / "cpp" / "source" / f"code_{lines}_lines.cpp"
        cpp_path.write_text(generate_cpp_code(lines))
    
    # CSV
    print("  Generating CSV files...")
    for rows in [100, 1000, 10000]:
        csv_path = base_dir / "documents" / "csv" / ("large" if rows > 5000 else "medium" if rows > 500 else "small") / f"data_{rows}_rows.csv"
        csv_path.write_text(generate_csv(rows))
    
    # Special files
    print("  Generating special files...")
    
    # High entropy (random)
    for size_kb in [10, 100]:
        random_path = base_dir / "special" / "high-entropy" / f"random_{size_kb}kb.bin"
        random_path.write_bytes(generate_random_bytes(size_kb))
    
    # Repeated patterns
    for pattern_size in [16, 64, 256]:
        pattern_path = base_dir / "special" / "edge-cases" / f"repeated_{pattern_size}b_pattern.bin"
        pattern_path.write_bytes(generate_repeated_pattern(pattern_size, 10000))
    
    # Low entropy
    for size_kb in [10, 100]:
        low_entropy_path = base_dir / "special" / "edge-cases" / f"low_entropy_{size_kb}kb.bin"
        low_entropy_path.write_bytes(generate_low_entropy(size_kb))
    
    # Empty file
    empty_path = base_dir / "special" / "edge-cases" / "empty.bin"
    empty_path.write_bytes(b"")
    
    # Single byte file
    single_path = base_dir / "special" / "edge-cases" / "single_byte.bin"
    single_path.write_bytes(b"\x42")
    
    # All zeros
    zeros_path = base_dir / "special" / "edge-cases" / "all_zeros_1mb.bin"
    zeros_path.write_bytes(b"\x00" * (1024 * 1024))
    
    # All 0xFF
    ones_path = base_dir / "special" / "edge-cases" / "all_ones_1mb.bin"
    ones_path.write_bytes(b"\xFF" * (1024 * 1024))
    
    print("\n✅ Benchmark corpus generated!")
    print(f"   Base directory: {base_dir}")
    
    # Print statistics
    total_files = sum(1 for _ in base_dir.rglob("*") if _.is_file())
    total_size = sum(f.stat().st_size for f in base_dir.rglob("*") if f.is_file())
    print(f"   Total files: {total_files}")
    print(f"   Total size: {total_size / (1024*1024):.2f} MB")


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Generate benchmark corpus for MoSqueeze")
    parser.add_argument("--output", "-o", default="./benchmark-corpus", help="Output directory")
    args = parser.parse_args()
    
    base_dir = Path(args.output)
    base_dir.mkdir(parents=True, exist_ok=True)
    
    generate_corpus(base_dir)


if __name__ == "__main__":
    main()
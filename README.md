# BioMXt
BioMxt (.bmxt) 🚀 High-performance, chunk-compressed matrix format for 10TB+ bioinformatics data. Optimized for web-based random access via C++/Zstd, with row-aligned indexing. 🧬 专为十万亿级生信矩阵设计的轻量化分块压缩格式。基于 C++ 与 Zstd，支持高效随机读取与 Web 端实时调取，是处理大规模矩阵数据的理想方案。

## 文件结构
```
BioMXt (*.bmxt)
Note: all offsets is absolute position in file.

Header: 64 bytes
	magic	4 bytes	"BMXt"
	version	2 bytes
	dtype	1 bytes	0: INT6, 1: INT32, 2: INT64, 3: FLOAT32, 4: FLOAT64
	algo	1 byte	0: Zstd(default), 1: Gzip, 2: LZ4
	nrow	4 bytes	0~4294967295
	ncol	4 bytes	0~4294967295
	chunk_size	4 bytes	0~4294967295	50000 as default
	chunk_table_offset	8 bytes
	names_table_offset	8 bytes
	reserved	36 byte
	
Chunk Data: variable length
	Chunk 1, variable length
	Chunk 2, variable length
	......
	Chunk N, variable length
	
String Pool: variable length
	String 1: variable length
	String 2: variable length
	......
	String N: variable length
	
Chunk Table: ceil(nrow * ncol / chunk_size) * 16 bytes
	Chunk 1:
		offset	8 bytes
		size	4 bytes
		reserved	4 bytes
	Chunk 2:
		offset	8 bytes
		size	4 bytes
		reserved	4 bytes
	Chunk N:
		offset	8 bytes
		size	4 bytes
		reserved	4 bytes
		
Names Table: (ncol + nrow) * 16 bytes
	Colname 1:
		offset	8 bytes
		size	4 bytes
		reserved	4 bytes
	Colname 2:
		offset	8 bytes
		size	4 bytes
		reserved	4 bytes
	......
	Colname N:
		offset	8 bytes
		size	4 bytes
		reserved	4 bytes
	Rowname 1:
		offset	8 bytes
		size	4 bytes
		reserved	4 bytes
	Rowname 2:
		offset	8 bytes
		size	4 bytes
		reserved	4 bytes
	......
	Rowname N:
		offset	8 bytes
		size	4 bytes
		reserved	4 bytes
```
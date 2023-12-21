#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>

#include "rom.h"
#include "text.h"


#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))


///////////////////////////////////////////////////////////////////////////
// rom

uint8_t prg[PRG_SIZE];

void read_rom() {
	FILE * f = fopen(ROM_NAME,"rb");
	if (!f) {
		printf("can't open %s: %s\n",ROM_NAME,strerror(errno));
		return;
	}
	for (unsigned i = 0; i < NES_HEADER_SIZE; i++) {
		fgetc(f);
	}
	fread(prg,1,PRG_SIZE,f);
	fclose(f);
}



////////////////////////////////////////////////////////////////////////////
// output file

FILE * of = NULL;

void open_output(const char * filename) {
	of = fopen(filename, "wb");
	if (!of) {
		printf("can't open %s: %s\n", filename, strerror(errno));
		return;
	}
}

void close_output() {
	fclose(of);
}





////////////////////////////////////////////////////////////////////////////
// dynamic array allocation

#define expand_array(ptr, cnt, max, initial_max) \
	if ((cnt) >= (max)) { \
		if ((max) == 0) \
			(max) = (initial_max); \
		else \
			(max) *= 2; \
		(ptr) = realloc((ptr), (max) * sizeof(*(ptr))); \
	}





//////////////////////////////////////////////////////////////////////////
// generic data heap

void * data_heap = NULL;
size_t data_heap_size = 0;
size_t data_heap_max = 0;

size_t append_data_heap_byte(unsigned b) {
	expand_array(data_heap, data_heap_size + 1, data_heap_max, 0x1000);
	((uint8_t *)data_heap)[data_heap_size] = b;
	return data_heap_size++;
}

void append_data_heap_byte_void(unsigned b) {
	append_data_heap_byte(b);
}

size_t append_data_heap(const void * data, size_t size) {
	expand_array(data_heap, data_heap_size + size, data_heap_max, 0x1000);
	memcpy(data_heap + data_heap_size, data, size);
	size_t offset = data_heap_size;
	data_heap_size += size;
	return offset;
}

size_t append_data_heap_string(const char * string) {
	size_t len = strlen(string);
	return append_data_heap(string, len+1);
}





///////////////////////////////////////////////////////////////////////////
// bins (areas of free rom space)

typedef struct {
	uint8_t bank;
	uint16_t start;
	uint16_t end; // inclusive range
	uint16_t size;
	uint16_t free;
} bin_t;

int bin_cmp(const void * a, const void * b) {
	const bin_t * aa = a;
	const bin_t * bb = b;
	
	unsigned aaa;
	unsigned bbb;
	if (aa->bank != bb->bank) {
		aaa = aa->bank;
		bbb = bb->bank;
	} else {
		aaa = aa->start;
		bbb = bb->start;
	}
	
	return (aaa > bbb) - (aaa < bbb);
}

void init_bin(bin_t * bin, unsigned bank, unsigned start, unsigned end) {
	bin->bank = bank;
	bin->start = start;
	bin->end = end;
	
	bin->size = end - start + 1;
	bin->free = bin->size;
}

void expand_bin_start(bin_t * bin, unsigned new) {
	bin->size += bin->start - new;
	bin->free += bin->start - new;
	
	bin->start = new;
}

void expand_bin_end(bin_t * bin, unsigned new) {
	bin->size += new - bin->end;
	bin->free += new - bin->end;
	
	bin->end = new;
}

void print_bin(const bin_t * bin) {
	printf("bank %X, $%04X-$%04X, size $%x, free $%x\n",
		bin->bank, bin->start, bin->end, bin->size, bin->free);
}



bin_t * bin_tbl = NULL;
size_t bin_cnt = 0;
size_t bin_max = 0;

size_t add_bin(unsigned bank, unsigned start, unsigned end) {
	bin_t * this_bin = NULL;
	
	// check for conditions where a new bin is not needed
	// basically, a new bin is not needed if it overlaps
	// or is directly adjacent to another bin.
	for (size_t i = 0; i < bin_cnt; i++) {
		bin_t * cmp_bin = &bin_tbl[i];
		if (bank == cmp_bin->bank) {
			if ((start >= cmp_bin->start && start <= (unsigned)cmp_bin->end+1) ||
				(end >= (unsigned)cmp_bin->start-1 && end <= cmp_bin->end)) {
				if (this_bin) {
					// we already have a bin. delete this unnecessary bin
					// and expand the one we're using
					expand_bin_start(this_bin, min(this_bin->start, cmp_bin->start));
					expand_bin_end(this_bin, max(this_bin->end, cmp_bin->end));
					memmove(&bin_tbl[i], &bin_tbl[i+1], sizeof(*bin_tbl) * (bin_cnt-- - i - 1));
					i--; // step back because we deleted a bin
				} else {
					// no bin found yet- set this as the current one and expand it
					this_bin = cmp_bin;
					expand_bin_start(this_bin, min(this_bin->start, start));
					expand_bin_end(this_bin, max(this_bin->end, end));
					start = this_bin->start;
					end = this_bin->end;
				}
			}
		}
	}
	
	// if any match, return
	if (this_bin) {
		return this_bin - bin_tbl;
	}
	
	// otherwise we need to add a new bin to the array
	expand_array(bin_tbl, bin_cnt+1, bin_max, 0x200);
	this_bin = &bin_tbl[bin_cnt++];
	init_bin(this_bin, bank, start, end);
	return this_bin - bin_tbl;
}

void print_all_bins() {
	for (size_t i = 0; i < bin_cnt; i++) {
		print_bin(&bin_tbl[i]);
	}
}


void read_free_space(const char * filename) {
	FILE * f = fopen(filename, "r");
	if (!f) {
		printf("can't open %s: %s\n", filename,strerror(errno));
		return;
	}
	
	char line[0x20];
	while (fgets(line, 0x20, f)) {
		unsigned bank;
		unsigned start;
		unsigned end;
		if (sscanf(line, "%x%x%x", &bank, &start, &end) == 3) {
			add_bin(bank, start, end);
		}
	}
	
	fclose(f);
}


void write_sections() {
	fprintf(of,
		"\t\n"
		"\t;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
		"\t;; Sections\n"
		"\t;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
		"\t\n"
		);
	for (size_t i = 0; i < bin_cnt; i++) {
		const bin_t * b = &bin_tbl[i];
		fprintf(of,
			"\t* = prg_offs(%u,$%04X)\n"
			"\t.logical $%04X\n"
			"\t.dsection section_%u_%zu\n"
			"\t.cerror * > $%04X, \"too long\"\n"
			"\t.here\n"
			"\t\n",
			b->bank, b->start,
			b->start,
			b->bank, i,
			b->end + 1
			);
	}
	fputc('\n',of);
	fputc('\n',of);
}




///////////////////////////////////////////////////////////////////////////
// huffman compression

typedef struct {
	void * parent;
	void * left;
	void * right;
	unsigned level;
	union {
		unsigned column; // within the level (only relevant if non-leaf node)
		unsigned data; // only relevant if leaf node
	};
	unsigned freq;
} huffman_node_t;

int huffman_node_cmp(const void * a, const void * b) {
	unsigned aa = ((const huffman_node_t *)a)->freq;
	unsigned bb = ((const huffman_node_t *)b)->freq;
	return (aa > bb) - (aa < bb);
}

int huffman_node_ptr_cmp(const void * a, const void * b) {
	const huffman_node_t * aa = *((const huffman_node_t **)a);
	const huffman_node_t * bb = *((const huffman_node_t **)b);
	return huffman_node_cmp(aa, bb);
}


#define HUFFMAN_SYMBOLS 0x100

huffman_node_t huffman_leaf_node_tbl[HUFFMAN_SYMBOLS];
huffman_node_t huffman_internal_node_tbl[HUFFMAN_SYMBOLS];
size_t huffman_internal_nodes = 0;
huffman_node_t * huffman_root_node = NULL;

unsigned huffman_code_tbl[HUFFMAN_SYMBOLS];
unsigned huffman_code_bits_tbl[HUFFMAN_SYMBOLS];
unsigned huffman_level_nodes[HUFFMAN_SYMBOLS]; // amount of non-leaf nodes on each level
unsigned huffman_max_level = 0;

// must call before using any huffman routines
void init_huffman() {
	huffman_internal_nodes = 0;
	huffman_root_node = NULL;
	memset(huffman_code_tbl, 0, sizeof(huffman_code_tbl));
	memset(huffman_code_bits_tbl, 0, sizeof(huffman_code_bits_tbl));
	memset(huffman_level_nodes,0,sizeof(huffman_level_nodes));
	huffman_max_level = 0;
	for (size_t i = 0; i < HUFFMAN_SYMBOLS; i++) {
		huffman_leaf_node_tbl[i].parent = NULL;
		huffman_leaf_node_tbl[i].left = NULL;
		huffman_leaf_node_tbl[i].right = NULL;
		huffman_leaf_node_tbl[i].level = 0;
		huffman_leaf_node_tbl[i].column = 0;
		
		huffman_leaf_node_tbl[i].data = i;
		huffman_leaf_node_tbl[i].freq = 0;
	}
}

// bump the frequency of a symbol
void count_huffman_symbol(unsigned i) {
	huffman_leaf_node_tbl[i].freq++;
}

// after frequencies have been counted, builds huffman tree
// and calculates all codes/bit counts for each symbol
void build_huffman_tree() {
	// build the initial queue by adding all leaf nodes
	huffman_node_t * node_queue[HUFFMAN_SYMBOLS * 2];
	size_t node_queue_start = 0;
	size_t node_queue_end = 0;
	for (size_t i = 0; i < HUFFMAN_SYMBOLS; i++) {
		if (huffman_leaf_node_tbl[i].freq) {
			node_queue[node_queue_end++] = &huffman_leaf_node_tbl[i];
		}
	}
	
	// build the tree
	while (node_queue_end - node_queue_start > 1) {
		qsort(
			node_queue + node_queue_start,
			node_queue_end - node_queue_start,
			sizeof(*node_queue),
			huffman_node_ptr_cmp);
		
		huffman_node_t * new_node = &huffman_internal_node_tbl[huffman_internal_nodes++];
		huffman_node_t * left = node_queue[node_queue_start + 0];
		huffman_node_t * right = node_queue[node_queue_start + 1];
		
		new_node->parent = NULL;
		new_node->left = left;
		new_node->right = right;
		new_node->level = 0;
		new_node->column = 0;
		new_node->freq = left->freq + right->freq;
		
		left->parent = new_node;
		right->parent = new_node;
		
		node_queue[node_queue_end++] = new_node;
		node_queue_start += 2;
	}
	huffman_root_node = node_queue[node_queue_end - 1];
	
	// traverse the tree to get the codes and levels for each symbol
	void traverse(huffman_node_t * node, unsigned level, unsigned code) {
		node->level = level;
		if (level > huffman_max_level)
			huffman_max_level = level;
		
		if (!node->left && !node->right) {
			// end of tree, mark the symbol
			huffman_code_tbl[node->data] = code;
			huffman_code_bits_tbl[node->data] = level;
		} else {
			// otherwise traverse further
			node->column = huffman_level_nodes[level]++;
			if (node->left) traverse(node->left, level+1, code | (0 << level));
			if (node->right) traverse(node->right, level+1, code | (1 << level));
		}
	}
	traverse(huffman_root_node, 0, 0);
}

// writes the huffman tree in binary form
void write_huffman_tree(void (*put)(unsigned)) {
	unsigned get_node_index(unsigned level, unsigned column) {
		// this function gets the node table index
		// note that each node here is two bytes long
		unsigned i = 0;
		for (unsigned l = 0; l < level; l++) {
			i += huffman_level_nodes[l];
		}
		return i + column;
	}
	
	int is_leaf(const huffman_node_t * node) {
		return !(node->left || node->right);
	}
	
	void output_node(const huffman_node_t * node, unsigned parent_index) {
		if (is_leaf(node)) {
			put(node->data);
		} else {
			unsigned this_index = get_node_index(node->level, node->column);
			unsigned offset = node->level
				? this_index - parent_index
				: 1;
			// if (offset) offset--;
			if (offset > 0x3f) {
				printf("warning: bad offset %02X\n",offset);
			}
			put(
				(is_leaf(node->left) ? 0x80 : 0x00) |
				(is_leaf(node->right) ? 0x40 : 0x00) |
				offset
				);
		}
	}
	
	// output first byte root node
	output_node(huffman_root_node, 0);
	output_node(huffman_root_node, 0); // also a padding byte
	
	// output the child pairs
	for (unsigned level = 0; level <= huffman_max_level; level++) {
		for (unsigned column = 0; column < huffman_level_nodes[level]; column++) {
			unsigned this_index = get_node_index(level, column);
			
			// find the node in the table
			huffman_node_t * n = NULL;
			for (size_t ni = 0; ni < huffman_internal_nodes; ni++) {
				n = &huffman_internal_node_tbl[ni];
				if (n->level == level && n->column == column) {
					break;
				}
			}
			
			// output left
			if (n->left) {
				output_node(n->left, this_index);
			} else {
				put(0);
			}
			
			// output right
			if (n->right) {
				output_node(n->right, this_index);
			} else {
				put(0);
			}
		}
	}
}

// encodes huffman-compressed data
void huffman_encode(const uint8_t * src, size_t src_size, void (*put)(unsigned)) {
	unsigned out_bit = 0;
	unsigned out_byte = 0;
	size_t si = 0;
	while (si < src_size) {
		unsigned b = src[si++];
		
		unsigned code = huffman_code_tbl[b];
		unsigned bits = huffman_code_bits_tbl[b];
		for (unsigned bit = 0; bit < bits; bit++) {
			unsigned bb = code & (1 << bit);
			if (bb) {
				out_byte |= 1 << out_bit;
			}
			if (++out_bit == 8) {
				put(out_byte);
				out_bit = 0;
				out_byte = 0;
			}
		}
	}
	if (out_bit) {
		put(out_byte);
	}
}



//////////////////////////////////////////////////////////////////////////
// data that will be fit into the bins

typedef struct {
	uint8_t bank; // bank this must be located in
	size_t index; // in data heap
	size_t size; // data size
	
	size_t label_index; // label of this data in the asm source
	
	uint8_t align; // must be aligned to x-byte boundary
	uint8_t huffman; // nonzero if this data should be huffman compressed
} data_block_t;

int data_block_cmp(const void * a, const void * b) {
	const size_t aa = ((const data_block_t *)a)->size;
	const size_t bb = ((const data_block_t *)b)->size;
	
	// note: descending order
	return (aa < bb) - (aa > bb);
}

int data_block_ptr_cmp(const void * a, const void * b) {
	const data_block_t * aa = *((const data_block_t **)a);
	const data_block_t * bb = *((const data_block_t **)b);
	
	return data_block_cmp(aa,bb);
}



data_block_t * data_block_tbl = NULL;
size_t data_block_cnt = 0;
size_t data_block_max = 0;

size_t add_data_block(unsigned bank, size_t index, size_t size, size_t label_index, unsigned align, int huffman) {
	expand_array(data_block_tbl, data_block_cnt + 1, data_block_max, 0x200);
	
	data_block_t * db = &data_block_tbl[data_block_cnt++];
	db->bank = bank;
	db->index = index;
	db->size = size;
	db->label_index = label_index;
	db->align = align;
	db->huffman = huffman;
	
	// if the data should be huffman compressed, go through it
	// and bump the symbol frequencies
	if (huffman) {
		const uint8_t * p = data_heap + index;
		for (size_t i = 0; i < size; i++) {
			count_huffman_symbol(p[i]);
		}
	}
	
	return db - data_block_tbl;
}


void create_huffman_table_data_block() {
	size_t huffman_table_index = data_heap_size;
	build_huffman_tree();
	write_huffman_tree(append_data_heap_byte_void);
	size_t huffman_table_size = data_heap_size - huffman_table_index;
	size_t label_index = append_data_heap_string("huffman_table");
	add_data_block(7, huffman_table_index, huffman_table_size, label_index, 2, 0);
}


void huffman_encode_data_blocks() {
	for (size_t i = 0; i < data_block_cnt; i++) {
		data_block_t * db = &data_block_tbl[i];
		if (db->huffman) {
			// allocate a new buffer for the old data- the encoding routine
			// might end up relocating the data heap when it appends its data,
			// making the pointer no longer valid
			void * buffer = malloc(db->size);
			memcpy(buffer, data_heap + db->index, db->size);
			size_t new_index = data_heap_size;
			huffman_encode(buffer, db->size, append_data_heap_byte_void);
			free(buffer);
			db->index = new_index;
			db->size = data_heap_size - new_index;
		}
	}
}




void write_data_block(size_t bin_index, data_block_t * db) {
	bin_t * b = &bin_tbl[bin_index];
	
	fprintf(of,
		"\t.section section_%u_%zu\n"
		"\t.align %u\n"
		"%s .byte ",
		b->bank, bin_index,
		db->align,
		(char *)data_heap + db->label_index
		);
	for (size_t i = 0; i < db->size; i++) {
		fprintf(of, "$%02X", *((uint8_t *)data_heap + db->index + i));
		if (i < db->size - 1) {
			fputc(',',of);
		}
	}
	b->free -= db->size;
	fprintf(of,
		"\n"
		"\t.send"
		"\n"
		);
}

void write_data_blocks_first_fit() {
	data_block_t * blocks[data_block_cnt];
	for (size_t i = 0; i < data_block_cnt; i++) {
		blocks[i] = &data_block_tbl[i];
	}
	qsort(blocks, data_block_cnt, sizeof(*blocks), data_block_ptr_cmp);
	
	for (size_t i = 0; i < data_block_cnt; i++) {
		data_block_t * db = blocks[i];
		size_t j = 0;
		for ( ; j < bin_cnt; j++) {
			size_t k = bin_cnt - j - 1;
			bin_t * b = &bin_tbl[k];
			if (b->bank == db->bank && db->size <= b->free) {
				write_data_block(k, db);
				break;
			}
		}
		if (j == bin_cnt) {
			printf("WARNING: could not find suitable bin for data block %s\n",
				(char *)data_heap + db->label_index);
		}
	}
}

void write_data_blocks() {
	fprintf(of,
		"\t\n"
		"\t;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
		"\t;; Data blocks\n"
		"\t;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
		"\t\n"
		"\t\n"
		);
	write_data_blocks_first_fit();
	fputc('\n',of);
	fputc('\n',of);
}





///////////////////////////////////////////////////////////////////////////
// data pointers

#define MAX_POINTERS 0x10

typedef uint16_t addr_t;

typedef struct {
	size_t data_block_index; // data block that contains the data
	
	unsigned ptr_cnt;
	addr_t lo[MAX_POINTERS]; // cpu address of low byte of pointer
	addr_t hi[MAX_POINTERS]; // cpu address of high byte of pointer
} data_pointer_t;

data_pointer_t * data_pointer_tbl = NULL;
size_t data_pointer_cnt = 0;
size_t data_pointer_max = 0;


size_t add_pointed_data(unsigned bank, unsigned ptr_cnt, const addr_t * lo, const addr_t * hi, size_t new_index, size_t new_size, unsigned align, int huffman, const char * label_prefix) {
	// create a data block for this data
	char label[0x40];
	size_t label_len = sprintf(label, "%s_%zu", label_prefix, data_pointer_cnt);
	size_t label_index = append_data_heap(label, label_len + 1);
	size_t data_block_index = add_data_block(bank, new_index, new_size, label_index, align, huffman);
	
	// create data pointer
	expand_array(data_pointer_tbl, data_pointer_cnt + 1, data_pointer_max, 0x200);
	
	data_pointer_t * dp = &data_pointer_tbl[data_pointer_cnt++];
	dp->data_block_index = data_block_index;
	dp->ptr_cnt = ptr_cnt;
	memcpy(dp->lo, lo, ptr_cnt * sizeof(*lo));
	memcpy(dp->hi, hi, ptr_cnt * sizeof(*hi));
	
	return dp - data_pointer_tbl;
}


void write_data_pointers() {
	fprintf(of,
		"\t\n"
		"\t;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
		"\t;; Data pointers\n"
		"\t;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
		"\t\n"
		"\t\n"
		);
	
	for (size_t i = 0; i < data_pointer_cnt; i++) {
		const data_pointer_t * dp = &data_pointer_tbl[i];
		const data_block_t * db = &data_block_tbl[dp->data_block_index];
		const char * label = data_heap + db->label_index;
		
		for (size_t j = 0; j < dp->ptr_cnt; j++) {
			fprintf(of,
				"\t* = prg_offs(%u,$%04X)\n"
				"\t.byte <%s\n"
				"\t* = prg_offs(%u,$%04X)\n"
				"\t.byte >%s\n",
				db->bank, dp->lo[j],
				label,
				db->bank, dp->hi[j],
				label
				);
		}
		fprintf(of,
			"\t\n");
	}
	
	fputc('\n',of);
	fputc('\n',of);
}






//////////////////////////////////////////////////////////////////////////
// charsets

void write_en_charset() {
	fprintf(of,
		"\t\n"
		"\t;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
		"\t;; Charset\n"
		"\t;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
		"\t\n"
		"\t.enc \"en-charset\"\n"
		);
	size_t i = 0;
	wchar_t c;
	while ((c = en_charset[i])) {
		fprintf(of,
			"\t.cdef \""
			);
		fputwc2utf8(c,of);
		fputwc2utf8(c,of);
		fprintf(of,
			"\",$%02zX\n",
			i++);
	}
	fprintf(of,
		"EN_CHARSET_SIZE = $%02zX\n",
		i);
	fputc('\n',of);
	fputc('\n',of);
}








///////////////////////////////////////////////////////////////////////////
// text strings

size_t add_text_string(unsigned bank, unsigned ptr_cnt, const addr_t * lo, const addr_t * hi, size_t new_index, size_t new_size, int detective) {
	// find the text strings in the rom and mark them as free space
	for (unsigned i = 0; i < ptr_cnt; i++) {
		unsigned loi = prg[get_prg_offset(bank, lo[i])];
		unsigned hii = prg[get_prg_offset(bank, hi[i])];
		unsigned addr = loi | (hii << 8);
		unsigned offset = get_prg_offset(bank, addr);
		size_t orig_size = 0;
		while (1) {
			if (prg[offset] == 0xff && prg[offset+1] == 0x00) {
				break;
			} else if (prg[offset] == 0xff || prg[offset] == 0xfe) {
				offset += 2;
				orig_size += 2;
			} else {
				offset += detective ? 2 : 1;
				orig_size += detective ? 2 : 1;
			}
		}
		add_bin(bank, addr, addr+orig_size+1);
	}
	
	// create a data pointer/block for the new string
	return add_pointed_data(bank, ptr_cnt, lo, hi, new_index, new_size, 1, 1, "text_string");
}


void read_text(const char * filename) {
	FILE * f = fopen(filename, "rb");
	if (!f) {
		printf("can't open %s: %s\n", filename, strerror(errno));
		return;
	}
	
	size_t start_index = -1;
	size_t size = 0;
	unsigned bank = -1;
	unsigned ptr_cnt = 0;
	addr_t lo[MAX_POINTERS];
	addr_t hi[MAX_POINTERS];
	unsigned detective = 0;
	
	wchar_t line[0x200];
	unsigned lineno = 0;
	while (1) {
		lineno++;
		
		// read line
		size_t line_len = 0;
		wchar_t c = WEOF;
		while (1) {
			c = fgetwcfromutf8(f);
			if (c == WEOF || c == L'\n') {
				line[line_len] = L'\0';
				break;
			} else {
				line[line_len++] = c;
			}
		}
		
		// if this is the eof or the start of a new string, flush the old one
		if (((!line_len && c == WEOF) || line[0] == L'@') && start_index != (size_t)-1 && size && bank != (unsigned)-1) {
			add_text_string(bank, ptr_cnt, lo, hi, start_index, size, detective);
			start_index = -1;
		}
		
		// if this is the eof, stop processing
		if (!line_len && c == WEOF) {
			break;
		}
		
		// if this is a comment line, skip
		if (line[0] == L'#') {
			continue;
		}
		
		// if this line is whitespace-only, skip
		int has_non_whitespace = 0;
		for (size_t i = 0; i < line_len; i++) {
			if (!iswspace(line[i])) {
				has_non_whitespace = 1;
				break;
			}
		}
		if (!has_non_whitespace) {
			 continue;
		}
		
		// parse the line
		if (line[0] == L'@') {
			// command line
			const wchar_t * wwhitespace = L" \n\r\t\v\f";
			unsigned tokcount = 0;
			bank = -1;
			ptr_cnt = 0;
			detective = 0;
			
			while (1) {
				wchar_t * tok = wcstok(tokcount++ ? NULL : line+1, wwhitespace);
				if (!tok) {
					break;
				}
				
				if (tokcount == 1) {
					bank = wcstoul(tok, NULL, 16);
				} else {
					unsigned this_lo;
					unsigned this_hi;
					if (swscanf(tok, L"%x/%x", &this_lo, &this_hi) == 2) {
						if (ptr_cnt == MAX_POINTERS) {
							printf("%s:%u: ignoring %04X/%04X\n", filename, lineno, this_lo, this_hi);
						} else {
							lo[ptr_cnt] = this_lo;
							hi[ptr_cnt] = this_hi;
							ptr_cnt++;
						}
					} else if (!wcscmp(tok,L"detective")) {
						detective = 1;
					} else {
						bank = -1;
						break;
					}
				}
			}
			
			if (bank == (unsigned)-1) {
				printf("%s:%u: malformed command line\n", filename, lineno);
			} else {
				start_index = data_heap_size;
				size = 0;
			}
		} else {
			// text line
			if (start_index != (size_t)-1) {
				size_t i = 0;
				while (i < line_len) {
					wchar_t c = line[i++];
					if (c == L'\\') {
						// code
						wchar_t cc = line[i++];
						if (cc >= L'0' && cc <= L'7') {
							// command
							append_data_heap_byte(0xff);
							append_data_heap_byte(cc - L'0');
							size += 2;
						} else if (cc == L'f') {
							// face setting
							wchar_t hic = line[i++];
							wchar_t loc = line[i++];
							if (!iswxdigit(loc) || !iswxdigit(hic)) {
								printf("%s:%u: bad face param %lc%lc\n", filename, lineno, hic,loc);
							}
							
							unsigned fid;
							swscanf(&line[i-2], L"%2X", &fid);
							append_data_heap_byte(0xfe);
							append_data_heap_byte(fid);
							size += 2;
						} else {
							printf("%s:%u: bad command param %lc\n", filename, lineno, cc);
						}
					} else {
						// regular text character
						append_data_heap_byte(get_en_char(c));
						size++;
					}
				}
			}
		}
	}
	
	fclose(f);
}





///////////////////////////////////////////////////////////////////////////
// nametables

// note: the lower 2 rows are attributes
#define NAMETABLE_WIDTH 32
#define NAMETABLE_HEIGHT 32

typedef uint8_t nametable_t[NAMETABLE_HEIGHT][NAMETABLE_WIDTH];

size_t add_nametable(unsigned bank, unsigned ptr_cnt, const addr_t * lo, const addr_t * hi, const nametable_t * nt) {
	// find the original nametables in the rom and mark them as free space
	for (unsigned i = 0; i < ptr_cnt; i++) {
		unsigned loi = prg[get_prg_offset(bank, lo[i])];
		unsigned hii = prg[get_prg_offset(bank, hi[i])];
		unsigned addr = loi | (hii << 8);
		unsigned offset = get_prg_offset(bank, addr);
		size_t orig_size = 0;
		while (1) {
			if (prg[offset] == 0xff) {
				offset++;
				unsigned c = prg[offset++];
				orig_size += 2;
				if (!c) {
					break;
				} else if (c >= 0x80) {
					offset += 2;
					orig_size += 2;
				} else if (c >= 3) {
					offset++;
					orig_size++;
				}
			} else {
				offset++;
				orig_size++;
			}
		}
		add_bin(bank, addr, addr+orig_size-1);
	}
	
	// rle-compress the nametable
	const uint8_t * ntp = (const uint8_t *) nt;
	size_t new_index = data_heap_size;
	size_t i = 0;
	while (i < sizeof(nametable_t)) {
		size_t left = sizeof(nametable_t) - i;
		
		// first check for a repeating byte pair of different bytes
		if (left > 2) {
			int did_byte_pair = 0;
			
			unsigned b0 = ntp[i+0];
			unsigned b1 = ntp[i+1];
			
			if (b0 != b1) {
				// check following repetitions
				unsigned count = 1;
				for (size_t j = i+2; j < sizeof(nametable_t)-1; j += 2) {
					unsigned c0 = ntp[j+0];
					unsigned c1 = ntp[j+1];
					
					if (b0 == c0 && b1 == c1) {
						count++;
					} else {
						break;
					}
				}
				
				// we only save bytes when this command uploads
				// more than 2 byte pairs
				while (count > 2) {
					did_byte_pair = 1;
					
					// we can only output up to 0x7f pairs
					unsigned this = min(count, 0x7f);
					append_data_heap_byte(0xff);
					append_data_heap_byte(this | 0x80);
					append_data_heap_byte(b0);
					append_data_heap_byte(b1);
					
					// step
					count -= this;
					i += this * 2;
				}
				
				// if we outputted byte pair commands reset now
				if (did_byte_pair) {
					continue;
				}
			}
		}
		
		// otherwise try standard single-byte runs
		unsigned b = ntp[i];
		unsigned count = 1;
		for (size_t j = i+1; j < sizeof(nametable_t); j++) {
			if (ntp[j] == b) {
				count++;
			} else {
				break;
			}
		}
		
		// byte 0xff has some special cases
		if (b == 0xff) {
			if (count == 1) {
				append_data_heap_byte(0xff);
				append_data_heap_byte(0x01);
				i++;
				continue;
			} else if (count == 2) {
				append_data_heap_byte(0xff);
				append_data_heap_byte(0x02);
				i += 2;
				continue;
			}
		}
		
		// otherwise work as normal
		// we only care about runs of 3 or more bytes
		if (count >= 3) {
			while (count >= 3) {
				// we can only output up to 0x7f bytes per run
				unsigned this = min(count, 0x7f);
				append_data_heap_byte(0xff);
				append_data_heap_byte(this);
				append_data_heap_byte(b);
				
				// step
				count -= this;
				i += this;
			}
		} else {
			// not enough for a run, just output the byte
			for (unsigned j = 0; j < count; j++) {
				append_data_heap_byte(b);
			}
			
			i += count;
		}
	}
	append_data_heap_byte(0xff);
	append_data_heap_byte(0x00);
	size_t new_size = data_heap_size - new_index;
	
	// add a data pointer/block for the nametable
	return add_pointed_data(bank, ptr_cnt, lo, hi, new_index, new_size, 1, 0, "nametable");
}


void decompress_nametable(nametable_t * nt, const uint8_t * data) {
	uint8_t * ntp = (uint8_t *)nt;
	size_t si = 0;
	size_t di = 0;
	
	while (1) {
		unsigned b = data[si++];
		if (b == 0xff) {
			unsigned c = data[si++];
			if (c == 0x00) {
				return;
			} else if (c == 0x01) {
				ntp[di++] = 0xff;
			} else if (c == 0x02) {
				ntp[di++] = 0xff;
				ntp[di++] = 0xff;
			} else if (c < 0x80) {
				b = data[si++];
				while (c--) {
					ntp[di++] = b;
				}
			} else {
				unsigned b1 = data[si++];
				unsigned b2 = data[si++];
				c &= 0x7f;
				while (c--) {
					ntp[di++] = b1;
					ntp[di++] = b2;
				}
			}
		} else {
			ntp[di++] = b;
		}
	}
}


void read_nametables(const char * filename) {
	FILE * f = fopen(filename, "rb");
	if (!f) {
		printf("can't open %s: %s\n", filename, strerror(errno));
		return;
	}
	
	nametable_t nt;
	unsigned bank = -1;
	unsigned ptr_cnt = 0;
	unsigned append = 0;
	addr_t lo[MAX_POINTERS];
	addr_t hi[MAX_POINTERS];
	
	wchar_t line[0x200];
	unsigned lineno = 0;
	while (1) {
		lineno++;
		
		// read line
		size_t line_len = 0;
		wchar_t c = WEOF;
		while (1) {
			c = fgetwcfromutf8(f);
			if (c == WEOF || c == L'\n') {
				line[line_len] = L'\0';
				break;
			} else {
				line[line_len++] = c;
			}
		}
		
		// if this is the eof or the start of a new string, flush the old one
		if (((!line_len && c == WEOF) || line[0] == L'@') && bank != (unsigned)-1) {
			add_nametable(bank, ptr_cnt, lo, hi, &nt);
			bank = -1;
		}
		
		// if this is the eof, stop processing
		if (!line_len && c == WEOF) {
			break;
		}
		
		// if this is a comment line, skip
		if (line[0] == L'#') {
			continue;
		}
		
		// if this line is whitespace-only, skip
		int has_non_whitespace = 0;
		for (size_t i = 0; i < line_len; i++) {
			if (!iswspace(line[i])) {
				has_non_whitespace = 1;
				break;
			}
		}
		if (!has_non_whitespace) {
			 continue;
		}
		
		// parse the line
		if (line[0] == L'@') {
			// command line
			const wchar_t * wwhitespace = L" \n\r\t\v\f";
			unsigned tokcount = 0;
			bank = -1;
			ptr_cnt = 0;
			append = 0;
			
			while (1) {
				wchar_t * tok = wcstok(tokcount++ ? NULL : line+1, wwhitespace);
				if (!tok) {
					break;
				}
				
				if (tokcount == 1) {
					bank = wcstoul(tok, NULL, 16);
				} else {
					unsigned this_lo;
					unsigned this_hi;
					if (swscanf(tok, L"%x/%x", &this_lo, &this_hi) == 2) {
						if (ptr_cnt == MAX_POINTERS) {
							printf("%s:%u: ignoring %04X/%04X\n", filename, lineno, this_lo, this_hi);
						} else {
							lo[ptr_cnt] = this_lo;
							hi[ptr_cnt] = this_hi;
							ptr_cnt++;
						}
					} else if (!wcscmp(tok, L"append")) {
						append = 1;
					} else {
						bank = -1;
						break;
					}
				}
			}
			
			if (bank == (unsigned)-1) {
				printf("%s:%u: malformed command line\n", filename, lineno);
			} else if (!ptr_cnt && append) {
				printf("%s:%u: append specified but no pointers\n", filename, lineno);
			} else if (ptr_cnt && append) {
				unsigned loi = prg[get_prg_offset(bank, lo[0])];
				unsigned hii = prg[get_prg_offset(bank, hi[0])];
				unsigned addr = loi | (hii << 8);
				unsigned offset = get_prg_offset(bank, addr);
				decompress_nametable(&nt, prg + offset);
			} else {
				memset(&nt, 0, sizeof(nt));
			}
		} else {
			// text line
			const wchar_t * wwhitespace = L" \n\r\t\v\f";
			const wchar_t * row_tok = wcstok(line, wwhitespace);
			if (row_tok) {
				const wchar_t * column_tok = wcstok(NULL, wwhitespace);
				if (column_tok) {
					wchar_t * text = wcstok(NULL, L"");
					if (text) {
						size_t wspan = wcsspn(text, wwhitespace);
						text += wspan;
						size_t textlen = wcslen(text);
						
						if (textlen) {
							unsigned row = wcstoul(row_tok, NULL, 10);
							unsigned column;
							if (!wcscmp(column_tok, L"c")) {
								column = (NAMETABLE_WIDTH - textlen) / 2;
							} else {
								column = wcstoul(column_tok, NULL, 10);
							}
							
							while (*text) {
								wchar_t ch = *text++;
								if (ch == L'\\') {
									wchar_t hic = *text++;
									wchar_t loc = *text++;
									if (!iswxdigit(loc) || !iswxdigit(hic)) {
										printf("%s:%u: bad tile param %lc%lc\n", filename, lineno, hic,loc);
									}
									
									unsigned tid;
									swscanf(text-2, L"%2X", &tid);
									nt[row][column++] = tid;
								} else {
									nt[row][column++] = get_en_char(ch);
								}
							}
						}
					}
				}
			}
			
		}
	}
	
	fclose(f);
}














////////////////////////////////////////////////////////////////////////////
// main

int main(int argc, char * argv[]) {
	setlocale(LC_ALL, "");
	
	if (argc != 5) {
		puts("usage: make-minky-data freespacedefs text nametables outfile");
		return EXIT_FAILURE;
	}
	const char * free_space_defs_name = argv[1];
	const char * text_name = argv[2];
	const char * nametables_name = argv[3];
	const char * out_name = argv[4];
	
	
	init_huffman();
	read_rom();
	read_free_space(free_space_defs_name);
	read_text(text_name);
	read_nametables(nametables_name);
	create_huffman_table_data_block();
	huffman_encode_data_blocks();
	
	open_output(out_name);
	write_en_charset();
	write_sections();
	write_data_blocks();
	write_data_pointers();
	close_output();
}

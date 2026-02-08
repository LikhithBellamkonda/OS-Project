#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<stdbool.h>

// Platform-specific macros
#ifdef _WIN32
    #define CLEAR_SCREEN "cls"
    #define SLEEP(x) system("timeout /t " #x " >nul")
#else
    #define CLEAR_SCREEN "clear"
    #define SLEEP(x) system("sleep " #x)
#endif

// Constants
#define MAX_PAGES 50
#define MAX_SEGMENTS 8
#define MAX_PROCESSES 5
#define PAGE_SIZE 4  // in KB
#define MEMORY_SIZE 64 // in KB

// ANSI color codes for better visualization
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

// Data structures
typedef struct {
    int page_no;
    int valid;
    int frame_no;
    int last_used;
    int reference_bit;
    int modify_bit;
} PageTableEntry;

typedef struct {
    int seg_no;
    int base;
    int limit;
    int valid;
} SegmentTableEntry;

typedef struct {
    int frame_no;
    int page_no;
    int process_id;
    int occupied;
    int reference_bit;
    int modify_bit;
    int age_counter;
    int load_time;
} Frame;

typedef struct {
    int pid;
    PageTableEntry page_table[MAX_PAGES];
    SegmentTableEntry seg_table[MAX_SEGMENTS];
    int page_count;
    int seg_count;
    char name[20];
} Process;

typedef struct {
    int page_no;
    int frame_no;
    int valid;
    int last_used;
} TLBEntry;

// Global variables
Frame *physical_memory = NULL;
Process processes[MAX_PROCESSES];
int process_count = 0;
int frame_count = 0;
int time_counter = 0;
int page_faults = 0;
int page_hits = 0;
int fifo_index = 0;
int clock_hand = 0;
TLBEntry tlb[32];
int tlb_size = 4;



// Function prototypes
void init_system();
void display_main_menu();
void simulate_paging();
void simulate_segmentation();
void simulate_page_replacement();
void display_memory();
void display_page_tables();
void display_segment_tables();
void generate_page_reference_string(int *ref_string, int length);
int fifo_replacement();
int lru_replacement();
int optimal_replacement(int *future_refs, int ref_count, int current_index);
int clock_replacement();
int get_free_frame();
void visualize_page_fault(int page_no, int frame_no, const char *algorithm);
void setup_memory_frames();
void add_new_process();
void clear_input_buffer();
void display_header(const char *title);
void simulate_tlb_system();
void display_tlb(int hit_page);
int search_tlb(int page_no);
void update_tlb(int page_no, int frame_no, int current_time);
void init_tlb();


// Function implementations
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void display_header(const char *title) {
    printf("\n" COLOR_CYAN "================================================================\n");
    printf("                                                                \n");
    printf("                   %-30s                 \n", title);
    printf("                                                                \n");
    printf("================================================================\n" COLOR_RESET);
}

void init_system() {
    srand((unsigned int)time(NULL));
    
    // Initialize processes
    process_count = 2;
    
    // Process 1
    strcpy(processes[0].name, "Process A");
    processes[0].pid = 1;
    processes[0].page_count = 8;
    processes[0].seg_count = 3;
    
    for (int i = 0; i < processes[0].page_count; i++) {
        processes[0].page_table[i].page_no = i;
        processes[0].page_table[i].valid = 0;
        processes[0].page_table[i].frame_no = -1;
        processes[0].page_table[i].last_used = -1;
        processes[0].page_table[i].reference_bit = 0;
        processes[0].page_table[i].modify_bit = rand() % 2;
    }
    
    processes[0].seg_table[0].seg_no = 0;
    processes[0].seg_table[0].base = 0;
    processes[0].seg_table[0].limit = 8;
    processes[0].seg_table[0].valid = 1;
    
    processes[0].seg_table[1].seg_no = 1;
    processes[0].seg_table[1].base = 8;
    processes[0].seg_table[1].limit = 12;
    processes[0].seg_table[1].valid = 1;
    
    processes[0].seg_table[2].seg_no = 2;
    processes[0].seg_table[2].base = 20;
    processes[0].seg_table[2].limit = 4;
    processes[0].seg_table[2].valid = 1;
    
    // Process 2
    strcpy(processes[1].name, "Process B");
    processes[1].pid = 2;
    processes[1].page_count = 6;
    processes[1].seg_count = 2;
    
    for (int i = 0; i < processes[1].page_count; i++) {
        processes[1].page_table[i].page_no = i;
        processes[1].page_table[i].valid = 0;
        processes[1].page_table[i].frame_no = -1;
        processes[1].page_table[i].last_used = -1;
        processes[1].page_table[i].reference_bit = 0;
        processes[1].page_table[i].modify_bit = rand() % 2;
    }
    
    processes[1].seg_table[0].seg_no = 0;
    processes[1].seg_table[0].base = 0;
    processes[1].seg_table[0].limit = 16;
    processes[1].seg_table[0].valid = 1;
    
    processes[1].seg_table[1].seg_no = 1;
    processes[1].seg_table[1].base = 16;
    processes[1].seg_table[1].limit = 8;
    processes[1].seg_table[1].valid = 1;
}

void setup_memory_frames() {
    printf("\n" COLOR_CYAN "Enter number of frames for paging (3-20): " COLOR_RESET);
    if (scanf("%d", &frame_count) != 1) {
        clear_input_buffer();
        frame_count = 5;
        printf(COLOR_YELLOW "Invalid input. Using default: 5 frames\n" COLOR_RESET);
    }
    clear_input_buffer();
    
    if (frame_count < 3) frame_count = 3;
    if (frame_count > 20) frame_count = 20;
    
    // Allocate memory for frames
    if (physical_memory != NULL) {
        free(physical_memory);
    }
    physical_memory = (Frame*)malloc(frame_count * sizeof(Frame));
    
    if (physical_memory == NULL) {
        printf(COLOR_RED "Memory allocation failed!\n" COLOR_RESET);
        frame_count = 0;
        return;
    }
    
    // Initialize frames
    for (int i = 0; i < frame_count; i++) {
        physical_memory[i].frame_no = i;
        physical_memory[i].occupied = 0;
        physical_memory[i].page_no = -1;
        physical_memory[i].process_id = -1;
        physical_memory[i].reference_bit = 0;
        physical_memory[i].modify_bit = 0;
        physical_memory[i].age_counter = 0;
        physical_memory[i].load_time = -1;
    }
    
    // Reset FIFO index and clock hand
    fifo_index = 0;
    clock_hand = 0;
    
    printf(COLOR_GREEN "Memory initialized with %d frames\n" COLOR_RESET, frame_count);
    SLEEP(1);
}

void display_main_menu() {
    system(CLEAR_SCREEN);
    display_header("MEMORY MANAGEMENT VISUALIZER");
    
    printf("\n" COLOR_GREEN "Main Menu:\n" COLOR_RESET);
    printf(COLOR_YELLOW "1." COLOR_RESET " Setup Memory Frames\n");
    printf(COLOR_YELLOW "2." COLOR_RESET " Simulate Paging System\n");
    printf(COLOR_YELLOW "3." COLOR_RESET " Simulate Segmentation System\n");
    printf(COLOR_YELLOW "4." COLOR_RESET " Simulate Page Replacement Algorithms\n");
    printf(COLOR_YELLOW "5." COLOR_RESET " Simulate TLB (Translation Lookaside Buffer)\n");
    printf(COLOR_YELLOW "6." COLOR_RESET " View Current Memory State\n");
    printf(COLOR_YELLOW "7." COLOR_RESET " View Page Tables\n");
    printf(COLOR_YELLOW "8." COLOR_RESET " View Segment Tables\n");
    printf(COLOR_YELLOW "9." COLOR_RESET " Add New Process\n");
    printf(COLOR_YELLOW "10." COLOR_RESET " Exit\n");

    
    printf("\n" COLOR_CYAN "Current Configuration: ");
    if (physical_memory == NULL) {
        printf("Memory not initialized\n" COLOR_RESET);
    } else {
        printf("%d frames allocated\n" COLOR_RESET, frame_count);
    }
    
    printf("\n" COLOR_CYAN "Enter your choice (1-10): " COLOR_RESET);

}

void display_memory() {
    if (physical_memory == NULL) {
        printf(COLOR_RED "\nMemory not initialized! Please setup memory frames first.\n" COLOR_RESET);
        return;
    }
    
    printf("\n" COLOR_MAGENTA "---------------------------------------------------------------------\n");
    printf("                    PHYSICAL MEMORY LAYOUT (%2d frames)               \n", frame_count);
    printf("-------------------------------------------------------------------------\n");
    printf(COLOR_YELLOW " Frame #  Page #   Process   R-bit   M-bit  Load T.  Status  \n" COLOR_RESET);
    printf(COLOR_MAGENTA "-------------------------------------------------------------------------\n" COLOR_RESET);
    
    for (int i = 0; i < frame_count; i++) {
        printf(COLOR_CYAN "   %2d   " COLOR_RESET, i);
        
        if (physical_memory[i].occupied) {
            printf(COLOR_GREEN "   P%-3d    P%-2d      %d       %d      %3d     Used  \n" COLOR_RESET,
                   physical_memory[i].page_no,
                   physical_memory[i].process_id,
                   physical_memory[i].reference_bit,
                   physical_memory[i].modify_bit,
                   physical_memory[i].load_time);
        } else {
            printf(COLOR_RED "   ---    ---     ---     ---    ---     Free  \n" COLOR_RESET);
        }
    }
    printf(COLOR_MAGENTA "-------------------------------------------------------------------------\n" COLOR_RESET);
    
    printf("\n" COLOR_YELLOW "Memory Usage: " COLOR_RESET);
    int used_frames = 0;
    for (int i = 0; i < frame_count; i++) {
        if (physical_memory[i].occupied) used_frames++;
    }
    printf("%d/%d frames (%.1f%%)\n", used_frames, frame_count, 
           (float)used_frames/frame_count*100);
}

void display_page_tables() {
    for (int p = 0; p < process_count; p++) {
        printf("\n" COLOR_CYAN "Process %d (%s) Page Table:\n" COLOR_RESET, processes[p].pid, processes[p].name);
        printf(COLOR_MAGENTA "-------------------------------------------------------------------------\n");
        printf(COLOR_YELLOW " Page #   Valid  Frame #  Last Use  R-bit  M-bit  In Mem. \n" COLOR_RESET);
        printf(COLOR_MAGENTA "-------------------------------------------------------------------------\n" COLOR_RESET);
        
        for (int i = 0; i < processes[p].page_count; i++) {
            printf(COLOR_CYAN "   %2d   " COLOR_RESET, processes[p].page_table[i].page_no);
            
            if (processes[p].page_table[i].valid) {
                printf(COLOR_GREEN "    Y      %2d      %3d       %d      %d",
                       processes[p].page_table[i].frame_no,
                       processes[p].page_table[i].last_used,
                       processes[p].page_table[i].reference_bit,
                       processes[p].page_table[i].modify_bit);
                
                // Check if actually in memory
                int in_memory = 0;
                if (physical_memory != NULL) {
                    for (int f = 0; f < frame_count; f++) {
                        if (physical_memory[f].occupied && 
                            physical_memory[f].page_no == processes[p].page_table[i].page_no &&
                            physical_memory[f].process_id == processes[p].pid) {
                            in_memory = 1;
                            break;
                        }
                    }
                }
                
                if (in_memory) {
                    printf("      Y    \n" COLOR_RESET);
                } else {
                    printf(COLOR_RED "      N    \n" COLOR_RESET);
                }
            } else {
                printf(COLOR_RED "    N      --     ---       -      -       N    \n" COLOR_RESET);
            }
        }
        printf(COLOR_MAGENTA "-------------------------------------------------------------------------\n" COLOR_RESET);
    }
}

void display_segment_tables() {
    for (int p = 0; p < process_count; p++) {
        printf("\n" COLOR_CYAN "Process %d (%s) Segment Table:\n" COLOR_RESET, processes[p].pid, processes[p].name);
        printf(COLOR_MAGENTA "---------------------------------------------------------------\n");
        printf(COLOR_YELLOW " Seg #    Base    Limit   Size    End Addr  Valid  \n" COLOR_RESET);
        printf(COLOR_MAGENTA "---------------------------------------------------------------\n" COLOR_RESET);
        
        for (int i = 0; i < processes[p].seg_count; i++) {
            printf(COLOR_CYAN "   %2d   " COLOR_RESET, processes[p].seg_table[i].seg_no);
            printf(COLOR_GREEN "  %4d    %4d   %4dK    %4d",
                   processes[p].seg_table[i].base,
                   processes[p].seg_table[i].limit,
                   processes[p].seg_table[i].limit,
                   processes[p].seg_table[i].base + processes[p].seg_table[i].limit);
            
            if (processes[p].seg_table[i].valid) {
                printf("      Y    \n" COLOR_RESET);
            } else {
                printf(COLOR_RED "      N    \n" COLOR_RESET);
            }
        }
        printf(COLOR_MAGENTA "---------------------------------------------------------------\n" COLOR_RESET);
    }
}

void simulate_paging() {
    if (physical_memory == NULL) {
        printf(COLOR_RED "\nMemory not initialized! Please setup memory frames first.\n" COLOR_RESET);
        printf("Press Enter to continue...");
        getchar();
        return;
    }
    
    system(CLEAR_SCREEN);
    display_header("PAGING SYSTEM SIMULATION");
    
    printf("\n" COLOR_YELLOW "Simulating Paging System...\n" COLOR_RESET);
    
    printf("\n" COLOR_CYAN "Logical to Physical Address Translation:\n" COLOR_RESET);
    printf("Page Size: %d KB\n", PAGE_SIZE);
    printf("Physical Memory: %d KB (%d frames)\n", MEMORY_SIZE, frame_count);
    printf("Frame Size: %d KB\n\n", PAGE_SIZE);
    
    // Show current memory state
    printf("Current Memory State:\n");
    display_memory();
    
    printf("\n" COLOR_CYAN "Address Translation Examples:\n" COLOR_RESET);
    
    // Generate some random address translations
    for (int i = 0; i < 3; i++) {
        int process_id = rand() % process_count;
        int logical_addr = rand() % (processes[process_id].page_count * PAGE_SIZE * 1024);
        int page_no = logical_addr / (PAGE_SIZE * 1024);
        int offset = logical_addr % (PAGE_SIZE * 1024);
        
        printf("\n" COLOR_MAGENTA "Example %d:\n" COLOR_RESET, i+1);
        printf("  Process: %s (ID: %d)\n", processes[process_id].name, processes[process_id].pid);
        printf("  Logical Address: %d\n", logical_addr);
        printf("  Page Number: %d (of %d)\n", page_no, processes[process_id].page_count);
        printf("  Offset: %d\n", offset);
        
        if (page_no < processes[process_id].page_count) {
            if (processes[process_id].page_table[page_no].valid) {
                int frame_no = processes[process_id].page_table[page_no].frame_no;
                int physical_addr = (frame_no * PAGE_SIZE * 1024) + offset;
                printf("  Page Status: " COLOR_GREEN "VALID (in memory)\n" COLOR_RESET);
                printf("  Frame Number: %d\n", frame_no);
                printf("  Physical Address: %d\n", physical_addr);
                printf("  Translation: %d -> %d\n", logical_addr, physical_addr);
            } else {
                printf("  Page Status: " COLOR_RED "INVALID (not in memory)\n" COLOR_RESET);
                printf("  " COLOR_RED "PAGE FAULT!" COLOR_RESET " Page needs to be loaded from disk.\n");
            }
        } else {
            printf("  " COLOR_RED "INVALID PAGE NUMBER!" COLOR_RESET " Page %d doesn't exist.\n", page_no);
        }
        
        if (i < 2) SLEEP(2);
    }
    
    printf("\n\n" COLOR_GREEN "Paging simulation complete!\n" COLOR_RESET);
    printf("\nPress Enter to continue...");
    getchar();
}

void simulate_segmentation() {
    system(CLEAR_SCREEN);
    display_header("SEGMENTATION SYSTEM SIMULATION");
    
    printf("\n" COLOR_YELLOW "Simulating Segmentation System...\n" COLOR_RESET);
    
    printf("\n" COLOR_CYAN "Segment-based Address Translation:\n" COLOR_RESET);
    printf("Each segment has variable size\n");
    printf("Base register contains starting address\n");
    printf("Limit register contains segment size\n\n");
    
    // Show segment tables
    display_segment_tables();
    
    printf("\n" COLOR_CYAN "Address Translation Examples:\n" COLOR_RESET);
    
    // Generate some random segment translations
    for (int i = 0; i < 3; i++) {
        int process_id = rand() % process_count;
        int seg_no = rand() % processes[process_id].seg_count;
        int offset = rand() % (processes[process_id].seg_table[seg_no].limit * 1024 * 2);
        int logical_addr = processes[process_id].seg_table[seg_no].base * 1024 + offset;
        
        printf("\n" COLOR_MAGENTA "Example %d:\n" COLOR_RESET, i+1);
        printf("  Process: %s (ID: %d)\n", processes[process_id].name, processes[process_id].pid);
        printf("  Segment Number: %d\n", seg_no);
        printf("  Segment Base: %d KB (%d bytes)\n", 
               processes[process_id].seg_table[seg_no].base,
               processes[process_id].seg_table[seg_no].base * 1024);
        printf("  Segment Limit: %d KB (%d bytes)\n", 
               processes[process_id].seg_table[seg_no].limit,
               processes[process_id].seg_table[seg_no].limit * 1024);
        printf("  Offset within segment: %d bytes\n", offset);
        printf("  Logical Address: %d\n", logical_addr);
        
        if (offset < processes[process_id].seg_table[seg_no].limit * 1024) {
            printf("  Access Status: " COLOR_GREEN "WITHIN LIMITS\n" COLOR_RESET);
            printf("  Physical Address: %d (same as logical in pure segmentation)\n", logical_addr);
            printf("  Translation: %d -> %d\n", logical_addr, logical_addr);
        } else {
            printf("  Access Status: " COLOR_RED "OUT OF BOUNDS\n" COLOR_RESET);
            printf("  " COLOR_RED "SEGMENTATION FAULT!" COLOR_RESET);
            printf(" Offset %d exceeds segment limit %d\n", 
                   offset, processes[process_id].seg_table[seg_no].limit * 1024);
        }
        
        if (i < 2) SLEEP(2);
    }
    
    printf("\n\n" COLOR_GREEN "Segmentation simulation complete!\n" COLOR_RESET);
    printf("\nPress Enter to continue...");
    getchar();
}

void visualize_page_fault(int page_no, int frame_no, const char *algorithm) {
    printf("\n" COLOR_RED "================================================================\n");
    printf("                        PAGE FAULT OCCURRED!\n");
    printf("================================================================\n" COLOR_RESET);
    printf("Requested Page: " COLOR_YELLOW "P%d" COLOR_RESET "\n", page_no);
    printf("Replacement Algorithm: " COLOR_CYAN "%s" COLOR_RESET "\n", algorithm);
    printf("Selected Frame for Replacement: " COLOR_MAGENTA "%d" COLOR_RESET "\n", frame_no);
    
    if (physical_memory[frame_no].occupied) {
        printf("Victim Page: " COLOR_RED "P%d" COLOR_RESET, physical_memory[frame_no].page_no);
        printf(" (Process P%d)\n", physical_memory[frame_no].process_id);
    }
}

void generate_page_reference_string(int *ref_string, int length) {
    printf("\n" COLOR_CYAN "Generated Reference String: " COLOR_RESET);
    for (int i = 0; i < length; i++) {
        // Generate references with some locality of reference
        if (i > 0 && rand() % 3 != 0) {
            // 66% chance to reference nearby pages
            ref_string[i] = (ref_string[i-1] + rand() % 3 - 1);
            if (ref_string[i] < 0) ref_string[i] = 0;
            if (ref_string[i] >= processes[0].page_count) 
                ref_string[i] = processes[0].page_count - 1;
        } else {
            ref_string[i] = rand() % processes[0].page_count;
        }
        printf("%d ", ref_string[i]);
    }
    printf("\n");
}

int get_free_frame() {
    for (int i = 0; i < frame_count; i++) {
        if (!physical_memory[i].occupied) {
            return i;
        }
    }
    return -1; // No free frame
}

int fifo_replacement() {
    int selected = fifo_index;
    
    // Find next occupied frame
    int checked = 0;
    while (checked < frame_count) {
        if (physical_memory[selected].occupied) {
            fifo_index = (selected + 1) % frame_count;
            return selected;
        }
        selected = (selected + 1) % frame_count;
        checked++;
    }
    
    // If no occupied frame found, return first frame
    fifo_index = 1 % frame_count;
    return 0;
}

int lru_replacement() {
    int lru_frame = 0;
    int min_time = time_counter + 1;
    bool found = false;
    
    for (int i = 0; i < frame_count; i++) {
        if (physical_memory[i].occupied) {
            if (physical_memory[i].load_time < min_time) {
                min_time = physical_memory[i].load_time;
                lru_frame = i;
                found = true;
            }
        }
    }
    
    return found ? lru_frame : 0;
}

int optimal_replacement(int *future_refs, int ref_count, int current_index) {
    int optimal_frame = 0;
    int farthest_use = -1;
    
    for (int i = 0; i < frame_count; i++) {
        if (physical_memory[i].occupied) {
            int next_use = ref_count + 1; // Default: never used again
            
            // Find next use of this page in future references
            for (int j = current_index; j < ref_count; j++) {
                if (future_refs[j] == physical_memory[i].page_no) {
                    next_use = j;
                    break;
                }
            }
            
            if (next_use > farthest_use) {
                farthest_use = next_use;
                optimal_frame = i;
            }
        }
    }
    return optimal_frame;
}

int clock_replacement() {
    int checked = 0;
    while (checked < frame_count * 2) { // Check all frames at most twice
        if (physical_memory[clock_hand].occupied) {
            if (physical_memory[clock_hand].reference_bit == 0) {
                int selected = clock_hand;
                clock_hand = (clock_hand + 1) % frame_count;
                return selected;
            } else {
                // Give second chance
                physical_memory[clock_hand].reference_bit = 0;
            }
        }
        clock_hand = (clock_hand + 1) % frame_count;
        checked++;
    }
    // Fallback: return current clock position
    return clock_hand;
}

void simulate_page_replacement() {
    if (physical_memory == NULL) {
        printf(COLOR_RED "\nMemory not initialized! Please setup memory frames first.\n" COLOR_RESET);
        printf("Press Enter to continue...");
        getchar();
        return;
    }
    
    system(CLEAR_SCREEN);
    display_header("PAGE REPLACEMENT ALGORITHMS");
    
    printf("\n" COLOR_YELLOW "Select Page Replacement Algorithm:\n" COLOR_RESET);
    printf(COLOR_CYAN "1." COLOR_RESET " FIFO (First-In First-Out)\n");
    printf(COLOR_CYAN "2." COLOR_RESET " LRU (Least Recently Used)\n");
    printf(COLOR_CYAN "3." COLOR_RESET " Optimal\n");
    printf(COLOR_CYAN "4." COLOR_RESET " Clock (Second Chance)\n");
    printf("\n" COLOR_YELLOW "Enter your choice (1-4): " COLOR_RESET);
    
    int algo_choice;
    if (scanf("%d", &algo_choice) != 1) {
        clear_input_buffer();
        algo_choice = 1;
    }
    clear_input_buffer();
    
    if (algo_choice < 1 || algo_choice > 4) {
        printf(COLOR_RED "Invalid choice! Using FIFO as default.\n" COLOR_RESET);
        algo_choice = 1;
    }
    
    const char *algo_names[] = {"FIFO", "LRU", "Optimal", "Clock"};
    
    // Ask for reference string length
    printf("\n" COLOR_CYAN "Enter length of reference string (5-30): " COLOR_RESET);
    int ref_length;
    if (scanf("%d", &ref_length) != 1) {
        clear_input_buffer();
        ref_length = 10;
    }
    clear_input_buffer();
    
    if (ref_length < 5) ref_length = 5;
    if (ref_length > 30) ref_length = 30;
    
    // Generate or input reference string
    printf("\n" COLOR_CYAN "Generate random reference string? (y/n): " COLOR_RESET);
    char choice;
    choice = getchar();
    clear_input_buffer();
    
    int *reference_string = (int*)malloc(ref_length * sizeof(int));
    if (reference_string == NULL) {
        printf(COLOR_RED "Memory allocation failed!\n" COLOR_RESET);
        return;
    }
    
    if (choice == 'y' || choice == 'Y') {
        generate_page_reference_string(reference_string, ref_length);
    } else {
        printf("\n" COLOR_CYAN "Enter %d page numbers (0-%d): \n", ref_length, processes[0].page_count - 1);
        for (int i = 0; i < ref_length; i++) {
            if (scanf("%d", &reference_string[i]) != 1) {
                reference_string[i] = 0;
            }
            if (reference_string[i] < 0) reference_string[i] = 0;
            if (reference_string[i] >= processes[0].page_count) 
                reference_string[i] = processes[0].page_count - 1;
        }
        clear_input_buffer();
        
        printf(COLOR_CYAN "Reference String: " COLOR_RESET);
        for (int i = 0; i < ref_length; i++) {
            printf("%d ", reference_string[i]);
        }
        printf("\n");
    }
    
    printf("\n" COLOR_CYAN "Starting %s Algorithm Simulation...\n" COLOR_RESET, algo_names[algo_choice-1]);
    printf("Initial Memory State:\n");
    display_memory();
    
    // Reset statistics and counters
    page_faults = 0;
    page_hits = 0;
    time_counter = 0;
    fifo_index = 0;
    clock_hand = 0;
    
    // Reset all page table entries
    for (int p = 0; p < process_count; p++) {
        for (int i = 0; i < processes[p].page_count; i++) {
            processes[p].page_table[i].valid = 0;
            processes[p].page_table[i].frame_no = -1;
            processes[p].page_table[i].last_used = -1;
            processes[p].page_table[i].reference_bit = 0;
        }
    }
    
    // Reset all frames
    for (int i = 0; i < frame_count; i++) {
        physical_memory[i].occupied = 0;
        physical_memory[i].page_no = -1;
        physical_memory[i].process_id = -1;
        physical_memory[i].reference_bit = 0;
        physical_memory[i].load_time = -1;
    }
    
    // Simulate page references
    for (int i = 0; i < ref_length; i++) {
        int page_no = reference_string[i];
        time_counter++;
        
        printf("\n" COLOR_MAGENTA "=" COLOR_RESET " Step %2d/%2d | Reference: Page %2d | Algorithm: %-7s " COLOR_MAGENTA "=" COLOR_RESET "\n", 
               i+1, ref_length, page_no, algo_names[algo_choice-1]);
        
        // Check if page is in memory
        int page_found = 0;
        int frame_no = -1;
        
        for (int f = 0; f < frame_count; f++) {
            if (physical_memory[f].occupied && physical_memory[f].page_no == page_no) {
                page_found = 1;
                frame_no = f;
                break;
            }
        }
        
        if (page_found) {
            // Page hit
            page_hits++;
            printf(COLOR_GREEN "* Page HIT! " COLOR_RESET);
            printf("Page %d found in frame %d\n", page_no, frame_no);
            
            // Update reference bit and last used time
            physical_memory[frame_no].reference_bit = 1;
            physical_memory[frame_no].load_time = time_counter;
            
            if (processes[0].page_table[page_no].valid) {
                processes[0].page_table[page_no].last_used = time_counter;
                processes[0].page_table[page_no].reference_bit = 1;
            }
        } else {
            // Page fault
            page_faults++;
            
            // Find frame for new page
            int free_frame = get_free_frame();
            
            if (free_frame != -1) {
                // Free frame available
                frame_no = free_frame;
                printf(COLOR_YELLOW "* Page FAULT! " COLOR_RESET);
                printf("Loading page %d into free frame %d\n", page_no, frame_no);
            } else {
                // Need to replace a page
                switch (algo_choice) {
                    case 1: // FIFO
                        frame_no = fifo_replacement();
                        break;
                    case 2: // LRU
                        frame_no = lru_replacement();
                        break;
                    case 3: // Optimal
                        frame_no = optimal_replacement(reference_string, ref_length, i+1);
                        break;
                    case 4: // Clock
                        frame_no = clock_replacement();
                        break;
                    default:
                        frame_no = fifo_replacement();
                }
                
                visualize_page_fault(page_no, frame_no, algo_names[algo_choice-1]);
                
                // Remove old page from page table
                if (physical_memory[frame_no].occupied) {
                    int old_page = physical_memory[frame_no].page_no;
                    int old_pid = physical_memory[frame_no].process_id;
                    
                    for (int p = 0; p < process_count; p++) {
                        if (processes[p].pid == old_pid) {
                            if (old_page < processes[p].page_count) {
                                processes[p].page_table[old_page].valid = 0;
                                processes[p].page_table[old_page].frame_no = -1;
                            }
                            break;
                        }
                    }
                    
                    printf("  Replaced " COLOR_RED "Page %d " COLOR_RESET, old_page);
                    printf("with " COLOR_GREEN "Page %d " COLOR_RESET, page_no);
                    printf("in frame %d\n", frame_no);
                }
            }
            
            // Load new page
            physical_memory[frame_no].occupied = 1;
            physical_memory[frame_no].page_no = page_no;
            physical_memory[frame_no].process_id = 1; // Assume process 1
            physical_memory[frame_no].reference_bit = 1;
            physical_memory[frame_no].modify_bit = rand() % 2;
            physical_memory[frame_no].load_time = time_counter;
            
            // Update page table for process 1
            if (page_no < processes[0].page_count) {
                processes[0].page_table[page_no].valid = 1;
                processes[0].page_table[page_no].frame_no = frame_no;
                processes[0].page_table[page_no].last_used = time_counter;
                processes[0].page_table[page_no].reference_bit = 1;
            }
        }
        
        display_memory();
        
        if (i < ref_length - 1) {
            printf("\nPress Enter for next reference...");
            getchar();
        }
    }
    
    // Display statistics
    printf("\n" COLOR_GREEN "================================================================\n");
    printf("                     SIMULATION RESULTS\n");
    printf("================================================================\n" COLOR_RESET);
    printf("Algorithm: %s\n", algo_names[algo_choice-1]);
    printf("Number of Frames: %d\n", frame_count);
    printf("Reference String Length: %d\n", ref_length);
    printf("Page Hits: %d\n", page_hits);
    printf("Page Faults: %d\n", page_faults);
    printf("Hit Ratio: %.2f%%\n", (float)page_hits/ref_length*100);
    printf("Fault Ratio: %.2f%%\n", (float)page_faults/ref_length*100);
    
    printf("\nFinal Memory State:\n");
    display_memory();
    
    printf("\nPress Enter to continue...");
    getchar();
    
    free(reference_string);
}

void add_new_process() {
    if (process_count >= MAX_PROCESSES) {
        printf(COLOR_RED "\nCannot add more processes. Maximum limit (%d) reached.\n" COLOR_RESET, MAX_PROCESSES);
        SLEEP(2);
        return;
    }
    
    system(CLEAR_SCREEN);
    display_header("ADD NEW PROCESS");
    
    processes[process_count].pid = process_count + 1;
    
    printf("\n" COLOR_CYAN "Enter process name: " COLOR_RESET);
    if (scanf("%19s", processes[process_count].name) != 1) {
        strcpy(processes[process_count].name, "Process");
    }
    clear_input_buffer();
    
    printf(COLOR_CYAN "Enter number of pages for %s (1-%d): ", 
           processes[process_count].name, MAX_PAGES);
    if (scanf("%d", &processes[process_count].page_count) != 1) {
        processes[process_count].page_count = 5;
    }
    clear_input_buffer();
    
    if (processes[process_count].page_count < 1) 
        processes[process_count].page_count = 1;
    if (processes[process_count].page_count > MAX_PAGES) 
        processes[process_count].page_count = MAX_PAGES;
    
    printf(COLOR_CYAN "Enter number of segments for %s (1-%d): ",
           processes[process_count].name, MAX_SEGMENTS);
    if (scanf("%d", &processes[process_count].seg_count) != 1) {
        processes[process_count].seg_count = 2;
    }
    clear_input_buffer();
    
    if (processes[process_count].seg_count < 1) 
        processes[process_count].seg_count = 1;
    if (processes[process_count].seg_count > MAX_SEGMENTS) 
        processes[process_count].seg_count = MAX_SEGMENTS;
    
    // Initialize page table
    for (int i = 0; i < processes[process_count].page_count; i++) {
        processes[process_count].page_table[i].page_no = i;
        processes[process_count].page_table[i].valid = 0;
        processes[process_count].page_table[i].frame_no = -1;
        processes[process_count].page_table[i].last_used = -1;
        processes[process_count].page_table[i].reference_bit = 0;
        processes[process_count].page_table[i].modify_bit = rand() % 2;
    }
    
    // Initialize segment table
    int base = 0;
    for (int i = 0; i < processes[process_count].seg_count; i++) {
        processes[process_count].seg_table[i].seg_no = i;
        processes[process_count].seg_table[i].base = base;
        printf(COLOR_CYAN "Enter size for segment %d (in KB, 1-20): ", i);
        if (scanf("%d", &processes[process_count].seg_table[i].limit) != 1) {
            processes[process_count].seg_table[i].limit = 4;
        }
        clear_input_buffer();
        
        if (processes[process_count].seg_table[i].limit < 1) 
            processes[process_count].seg_table[i].limit = 1;
        if (processes[process_count].seg_table[i].limit > 20) 
            processes[process_count].seg_table[i].limit = 20;
            
        processes[process_count].seg_table[i].valid = 1;
        base += processes[process_count].seg_table[i].limit;
    }
    
    process_count++;
    printf(COLOR_GREEN "\nProcess '%s' added successfully with PID %d!\n" COLOR_RESET, 
           processes[process_count-1].name, processes[process_count-1].pid);
    
    printf("\nPress Enter to continue...");
    getchar();
}

int main() {
    init_system();
    
    int choice;
    do {
        display_main_menu();
        if (scanf("%d", &choice) != 1) {
            clear_input_buffer();
            printf(COLOR_RED "Invalid input! Please enter a number.\n" COLOR_RESET);
            SLEEP(1);
            continue;
        }
        clear_input_buffer();
        
        switch (choice) {
            case 1:
                setup_memory_frames();
                break;
            case 2:
                simulate_paging();
                break;
            case 3:
                simulate_segmentation();
                break;
            case 4:
                simulate_page_replacement();
                break;
            case 5:
                simulate_tlb_system();
                break;
            case 6:
                system(CLEAR_SCREEN);
                display_header("MEMORY STATE");
                display_memory();
                printf("\nPress Enter to continue...");
                getchar();
                break;
            case 7:
                system(CLEAR_SCREEN);
                display_header("PAGE TABLES");
                display_page_tables();
                printf("\nPress Enter to continue...");
                getchar();
                break;
            case 8:
                system(CLEAR_SCREEN);
                display_header("SEGMENT TABLES");
                display_segment_tables();
                printf("\nPress Enter to continue...");
                getchar();
                break;
            case 9:
                add_new_process();
                break;
            case 10:
                system(CLEAR_SCREEN);
                display_header("EXITING MEMORY MANAGEMENT VISUALIZER");
                printf(COLOR_GREEN "\nThank you for using the Memory Management Visualizer!\n" COLOR_RESET);
                printf(COLOR_YELLOW "Goodbye!\n\n" COLOR_RESET);
                break;
            default:
                printf(COLOR_RED "Invalid choice! Please enter 1-10.\n" COLOR_RESET);

                SLEEP(1);
        }
    } while (choice != 10);

    
    // Clean up
    if (physical_memory != NULL) {
        free(physical_memory);
    }
    
    return 0;
}

// TLB Function Implementations

void init_tlb() {
    for (int i = 0; i < 32; i++) {
        tlb[i].page_no = -1;
        tlb[i].frame_no = -1;
        tlb[i].valid = 0;
        tlb[i].last_used = -1;
    }
}

int search_tlb(int page_no) {
    for (int i = 0; i < tlb_size; i++) {
        if (tlb[i].valid && tlb[i].page_no == page_no) {
            return i; // TLB Hit
        }
    }
    return -1; // TLB Miss
}

void update_tlb(int page_no, int frame_no, int current_time) {
    // Check if empty slot exists
    int empty_slot = -1;
    for (int i = 0; i < tlb_size; i++) {
        if (!tlb[i].valid) {
            empty_slot = i;
            break;
        }
    }
    
    if (empty_slot != -1) {
        // Use empty slot
        tlb[empty_slot].page_no = page_no;
        tlb[empty_slot].frame_no = frame_no;
        tlb[empty_slot].valid = 1;
        tlb[empty_slot].last_used = current_time;
    } else {
        // LRU Replacement
        int lru_index = 0;
        int min_time = tlb[0].last_used;
        
        for (int i = 1; i < tlb_size; i++) {
            if (tlb[i].last_used < min_time) {
                min_time = tlb[i].last_used;
                lru_index = i;
            }
        }
        
        tlb[lru_index].page_no = page_no;
        tlb[lru_index].frame_no = frame_no;
        tlb[lru_index].valid = 1;
        tlb[lru_index].last_used = current_time;
    }
}

void display_tlb(int hit_page) {
    printf("\n" COLOR_CYAN "Current TLB State (Size: %d):" COLOR_RESET "\n", tlb_size);
    printf(COLOR_MAGENTA "+-------+---------+---------+-----------+\n");
    printf("| Entry | Page No | Frame No| Last Used |\n");
    printf("+-------+---------+---------+-----------+\n" COLOR_RESET);
    
    for (int i = 0; i < tlb_size; i++) {
        if (tlb[i].valid) {
            if (tlb[i].page_no == hit_page) {
                printf(COLOR_GREEN "|   %d   |    %2d   |    %2d   |    %3d    | <--- Hit\n" COLOR_RESET, 
                       i, tlb[i].page_no, tlb[i].frame_no, tlb[i].last_used);
            } else {
                printf("|   %d   |    %2d   |    %2d   |    %3d    |\n", 
                       i, tlb[i].page_no, tlb[i].frame_no, tlb[i].last_used);
            }
        } else {
            printf("|   %d   |    --   |    --   |    ---    |\n", i);
        }
    }
    printf(COLOR_MAGENTA "+-------+---------+---------+-----------+\n" COLOR_RESET);
}

void simulate_tlb_system() {
    system(CLEAR_SCREEN);
    display_header("TLB SIMULATION");
    
    int hit_time, miss_time, ref_len;
    
    // Configuration
    printf("\n" COLOR_CYAN "TLB Configuration:\n" COLOR_RESET);
    printf("Enter TLB Size (2-32): ");
    if (scanf("%d", &tlb_size) != 1) tlb_size = 4;
    if (tlb_size < 2) tlb_size = 2;
    if (tlb_size > 32) tlb_size = 32;
    clear_input_buffer();
    
    printf("Enter TLB Hit Time (ns): ");
    if (scanf("%d", &hit_time) != 1) hit_time = 10;
    clear_input_buffer();
    
    printf("Enter Main Memory Access Time (ns): ");
    if (scanf("%d", &miss_time) != 1) miss_time = 100;
    clear_input_buffer();
    
    printf("Enter Reference String Length (5-20): ");
    if (scanf("%d", &ref_len) != 1) ref_len = 10;
    clear_input_buffer();
    
    // Generate Reference String
    int *ref_string = (int*)malloc(ref_len * sizeof(int));
    srand(time(NULL));
    printf("\n" COLOR_YELLOW "Reference String: " COLOR_RESET);
    for (int i = 0; i < ref_len; i++) {
        ref_string[i] = rand() % 10; // Pages 0-9
        printf("%d ", ref_string[i]);
    }
    printf("\n");
    
    // Simulation
    init_tlb();
    int tlb_hits = 0, tlb_misses = 0;
    int total_time = 0;
    
    printf("\n" COLOR_GREEN "Starting Simulation..." COLOR_RESET "\n");
    SLEEP(1);
    
    for (int i = 0; i < ref_len; i++) {
        int page = ref_string[i];
        int time_step = i + 1;
        int frame = page * 2 + 1; // Dummy frame mapping
        
        printf("\n" COLOR_CYAN "Step %d: Accessing Page %d" COLOR_RESET "\n", time_step, page);
        
        int tlb_index = search_tlb(page);
        
        if (tlb_index != -1) {
            // Hit
            tlb_hits++;
            total_time += hit_time;
            printf(COLOR_GREEN "  -> TLB HIT! Time: %dns\n" COLOR_RESET, hit_time);
            
            // Update usage
            tlb[tlb_index].last_used = time_step;
            display_tlb(page);
        } else {
            // Miss
            tlb_misses++;
            total_time += (hit_time + miss_time); // TLB search + Memory access
            printf(COLOR_RED "  -> TLB MISS! Time: %d + %d = %dns\n" COLOR_RESET, hit_time, miss_time, hit_time + miss_time);
            
            update_tlb(page, frame, time_step);
            display_tlb(-1);
        }
        
        SLEEP(1);
    }
    
    // Results
    printf("\n" COLOR_YELLOW "========================================\n");
    printf("           SIMULATION RESULTS           \n");
    printf("========================================\n" COLOR_RESET);
    printf("Total Accesses: %d\n", ref_len);
    printf("TLB Hits:       " COLOR_GREEN "%d" COLOR_RESET "\n", tlb_hits);
    printf("TLB Misses:     " COLOR_RED "%d" COLOR_RESET "\n", tlb_misses);
    
    float hit_ratio = (float)tlb_hits / ref_len;
    printf("Hit Ratio:      %.2f%%\n", hit_ratio * 100);
    
    float avg_time = (float)total_time / ref_len;
    printf("Avg Access Time: %.2fns\n", avg_time);
    
    // Ideal vs Actual
    printf("\n" COLOR_CYAN "Performance Analysis:" COLOR_RESET "\n");
    printf("Without TLB:     %d ns (Assuming %d ns access)\n", ref_len * miss_time, miss_time);
    printf("With TLB:        %d ns\n", total_time);
    printf("Speedup:         %.2fx\n", (float)(ref_len * miss_time) / total_time);
    
    printf("\nPress Enter to continue...");
    getchar();
    free(ref_string);
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <wait.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#ifndef IN_FNAME
#define IN_FNAME "yeangi_proj2_input"
#endif

#ifndef OUT_FNAME
#define OUT_FNAME "yeangi_proj2_output_"
#endif

#ifndef NUM_REFERENCES
#define NUM_REFERENCES 100
#endif

typedef struct Page {
    int page;
    time_t timestamp;
    bool referenced;
} Page;

void SelectionPrompt(size_t* frame_selection, size_t* algorithm_selection);
void GenerateRefString();
void GrabRefString(int reference_string[]);
char* int_to_char(int x);

bool FIFO(Page result[][NUM_REFERENCES], Page current[], int page, int num_frames, size_t current_reference, time_t timestamp);
bool LRU(Page result[][NUM_REFERENCES], Page current[], int page, int num_frames, size_t current_reference, time_t timestamp);
bool SecondChance(Page result[][NUM_REFERENCES], Page current[], int page, int num_frames, size_t current_reference, time_t timestamp);

void HandleSimulation(int reference_string[], int alg_type, int num_frames, bool p_stdout);
void PrintCurrentPT(Page result[][NUM_REFERENCES], int num_frames, bool faults[], size_t num_faults, int reference_string[NUM_REFERENCES]);
void WriteResultPT(Page result[][NUM_REFERENCES], int num_frames, bool faults[], size_t num_faults, int reference_string[NUM_REFERENCES], int alg_type);

int main() {
    // remove any previous files
    int child = fork();
    if (!child) {
        // child
        system("rm -f *.txt");
        exit(0);
    } else {
        int status;
        waitpid(child, &status, 0);
    }

    // prompt asking 4 vs 8 pg frame, pick between 3 algorithm types
    size_t frame_selection = -1, algorithm_selection = -1;
    SelectionPrompt(&frame_selection, &algorithm_selection);

    // randomly generate NUM_REFERENCES ints b/w 0 and 15 into file
    GenerateRefString();
    
    // read from file
    int reference_string[NUM_REFERENCES];
    GrabRefString(reference_string);

    // for every algorithm:
    for (size_t alg_type = 1; alg_type <= 3; alg_type++) {
        // for 4 AND 8 pg frame:
        for (size_t frame_type = 1; frame_type <= 2; frame_type++) {
            int num_frames = 4;
            if (frame_type == 2)
                num_frames = 8;

            bool p_stdout = false;
            if (alg_type == algorithm_selection && frame_type == frame_selection)
                p_stdout = true;

            HandleSimulation(reference_string, alg_type, num_frames, p_stdout);
        }
    }
}

void HandleSimulation(int reference_string[NUM_REFERENCES], int alg_type, int num_frames, bool p_stdout) {
    Page result[num_frames][NUM_REFERENCES];
    
    Page x = {.page = -1, .timestamp = -1, .referenced = false};
    for (size_t i = 0; i < num_frames; i++)
        for (size_t j = 0; j < NUM_REFERENCES; j++)
            result[i][j] = x;

    Page current[num_frames];
    for (size_t i = 0; i < num_frames; i++)
        current[i] = x;

    bool faults[NUM_REFERENCES];
    for (size_t i = 0; i < NUM_REFERENCES; i++)
        faults[i] = false;

    time_t current_time = 0;
    size_t num_faults = 0;
    for (size_t i = 0; i < NUM_REFERENCES; i++) {
        // read number of referenced page
        int page = reference_string[i];

        if (alg_type == 1)
            faults[i] = FIFO(result, current, page, num_frames, i, current_time+i);
        else if (alg_type == 2)
            faults[i] = LRU(result, current, page, num_frames, i, current_time+i);
        else
            faults[i] = SecondChance(result, current, page, num_frames, i, current_time+i);
        
        if (faults[i])
            num_faults++;
    }
    // display system time, the page referenced, and whether fault occurred
    // calculate total # page faults for each algorithm/page_frame pair
    // if is algorithm selected && page frame selected, print to stdout
    if (p_stdout) {
        printf("===================\n");
        PrintCurrentPT(result, num_frames, faults, num_faults, reference_string);
    }
    // write results for 4/8 pg frame in respective file (use chmod)
    WriteResultPT(result, num_frames, faults, num_faults, reference_string, alg_type);
}

void WriteResultPT(Page result[][NUM_REFERENCES], int num_frames, bool faults[], size_t num_faults, int reference_string[NUM_REFERENCES], int alg_type) {
    // create filename with proper extension
    char* frames_ext = malloc(sizeof(char) * (strlen("4frames.txt") + 1));
    strcpy(frames_ext, "4frames.txt");
    if (num_frames == 8)
        frames_ext[0] = '8';
    
    size_t len = strlen(OUT_FNAME) + strlen(frames_ext) + 1;
    char* fname = malloc(sizeof(char) * len);
    strcpy(fname, OUT_FNAME);
    strcat(fname, frames_ext);

    // create file descriptor
    FILE* f;
    int fd;
    // if exists, append mode
    if (f = fopen(fname, "r")) {
        fd = open(fname, O_APPEND | O_WRONLY);
    }
    
    // otherwise, create mode
    else {
        fd = open(fname, O_CREAT | O_WRONLY);
        f = fopen(fname, "r");

        chmod(fname, 0777);
    }

    // create STDOUT fd backup
    int backup = dup(1);

    // redirect STDOUT fd to FILE fd
    int x = dup2(fd, 1);

    // write to STDOUT (which is pointing to file)
    if (alg_type == 1)
        printf("================\n======FIFO======\n================\n");

    else if (alg_type == 2)
        printf("===============\n======LRU======\n===============\n");
    else
        printf("==============\n======SC======\n==============\n");
    PrintCurrentPT(result, num_frames, faults, num_faults, reference_string);

    // repoint STDOUT fd to backup
    int y = dup2(backup, 1);
    
    // free allocated memory
    free(fname);
    free(frames_ext);

    // close appropriate fds
    fclose(f);
    close(fd);
    close(backup);
}

bool FIFO(Page result[][NUM_REFERENCES], Page current[], int page, int num_frames, size_t current_reference, time_t timestamp) {
    // check if already exists
    for (size_t i = 0; i < num_frames; i++)
        if (current[i].page == page) {
            // since exists, flip referenced flag
            current[i].referenced = true;
            current[i].timestamp = timestamp;

            // since exists, copy previous P.T to current P.T
            for (size_t i = 0; i < num_frames; i++)
                result[i][current_reference] = current[i];
            
            // since exists, no fault
            return false;
        }

    // use system call to generate time stamp
    Page p = {.page = page, .timestamp = timestamp, .referenced = false};

    // update current page table (shift and queue)
    for (size_t i = num_frames-1; i > 0; i--)
        current[i] = current[i-1];
    current[0] = p;

    // send updates to result
    for (size_t i = 0; i < num_frames; i++)
        result[i][current_reference] = current[i];

    // since didn't exist, fault
    return true;
}

bool LRU(Page result[][NUM_REFERENCES], Page current[], int page, int num_frames, size_t current_reference, time_t timestamp) {
    // check if already exists
    for (size_t i = 0; i < num_frames; i++)
        if (current[i].page == page) {
            // since exists, flip referenced flag
            current[i].referenced = true;
            current[i].timestamp = timestamp;

            // since exists, copy previous P.T to current P.T
            for (size_t i = 0; i < num_frames; i++)
                result[i][current_reference] = current[i];
            
            // since exists, no fault
            return false;
        }

    // use system call to generate time stamp
    Page p = {.page = page, .timestamp = timestamp, .referenced = false};

    // update current page table (find LRU candidate, replace with new)
    int empty_slot = -1;
    int seek = -1;
    time_t oldest_timestamp = -1;
    for (size_t i = 0; i < num_frames; i++) {
        if (current[i].page == -1) {
            empty_slot = i;
            break;
        }
        else
            if (oldest_timestamp == -1 || oldest_timestamp > current[i].timestamp) {
                seek = i;
                oldest_timestamp = current[i].timestamp;
            }
    }

    // case: empty slot to be filled
    if (empty_slot != -1)
        current[empty_slot] = p;
    // case: replacing oldest timestamp
    else
        current[seek] = p;

    // send updates to result
    for (size_t i = 0; i < num_frames; i++)
        result[i][current_reference] = current[i];

    // since didn't exist, fault
    return true;
}

bool SecondChance(Page result[][NUM_REFERENCES], Page current[], int page, int num_frames, size_t current_reference, time_t timestamp) {
    // check if already exists
    for (size_t i = 0; i < num_frames; i++)
        if (current[i].page == page) {
            // since exists, flip referenced flag and update timestamp
            current[i].referenced = true;
            current[i].timestamp = timestamp;

            // since exists, copy previous P.T to current P.T
            for (size_t i = 0; i < num_frames; i++)
                result[i][current_reference] = current[i];
            
            // since exists, no fault
            return false;
        }

    // use system call to generate time stamp
    Page p = {.page = page, .timestamp = timestamp, .referenced = false};

    // update current page table (find first candidate, appropriately shift, and queue)
    // find first candidate
    int seek = -1;
    int empty_slot = -1;
    for (int i = num_frames-1; i >= 0; i--) {
        if (current[i].page == -1)
            empty_slot = i;
        else
            if (!current[i].referenced) {
                seek = i;
                break;
            }
    }

    // case: candidate is empty slot
    if (empty_slot != -1) {
        for (size_t i = empty_slot; i > 0; i--)
            current[i] = current[i-1];
        current[0] = p;
    }
    
    // case: no empty slots to fill
    else {
        // if all referenced and filled, just remove FIFO candidate
        if (seek == -1)
            seek = num_frames-1;

        // shift everything behind seek
        for (size_t i = seek; i > 0; i--)
            current[i] = current[i-1];
        
        // queue
        current[0] = p;
    }
    
    // send updates to result
    for (size_t i = 0; i < num_frames; i++)
        result[i][current_reference] = current[i];

    // since didn't exist, fault
    return true;
}

void PrintCurrentPT(Page result[][NUM_REFERENCES], int num_frames, bool faults[], size_t num_faults, int reference_string[NUM_REFERENCES]) {
    for (size_t i = 0; i < NUM_REFERENCES; i++) {
        printf("Requesting Page Number %d\n", reference_string[i]);
        for (size_t x = 0; x < 4; x++) {
            if (x == 0)
                printf("Fault?     : %d", faults[i]);
            else if (x == 1)
                printf("R-bit      : ");
            else if (x == 2)
                printf("Page Table : ");
            else
                printf("Timestamp  : ");

            for (size_t j = 0; j < num_frames; j++) {
                if (x == 1){ 
                    printf("%d ", result[j][i].referenced);
                }
                else if (x == 2) {
                    if (result[j][i].page != -1)
                        printf("%d ", result[j][i].page);
                    else
                        printf("- ");
                }
                else if (x == 3) {
                    if (result[j][i].timestamp != -1)
                        printf("%ld ", result[j][i].timestamp);
                    else
                        printf("- ");
                }
            }
            printf("\n");
        }
        printf("=====================================================\n");
    }
    printf("Number Faults: %ld\n", num_faults);
}

void SelectionPrompt(size_t* frame_selection, size_t* algorithm_selection) {
    char* frame_prompt = "Number of Page Frames: \n"
                        "1. 4 page frames\n"
                        "2. 8 page frames\n"
                        "Selection: ";
    do {
        printf("%s", frame_prompt);
        scanf("%ld", frame_selection);
    } while(*frame_selection != 1 && *frame_selection != 2);
    
    char* algorithm_prompt = "\nAlgorithm Type: \n"
                        "1. FIFO\n"
                        "2. LRU\n"
                        "3. Second Chance\n"
                        "Selection: ";
    do {
        printf("%s", algorithm_prompt);
        scanf("%ld", algorithm_selection);
    } while(*algorithm_selection < 1 && *algorithm_selection > 3);

    printf("\n");
}

void GenerateRefString() {
    srand(time(NULL));
    char fname[strlen(IN_FNAME) + strlen(".txt") + 1];
    strcpy(fname, IN_FNAME);
    strcat(fname, ".txt");

    FILE* fptr = fopen(fname, "w");
    for (size_t i = 0; i < NUM_REFERENCES; i++) {
        char* page = int_to_char(rand() % 16);
        fwrite(page, sizeof(char), strlen(page), fptr);
        fwrite(" ", sizeof(char), 1, fptr);
    }
    fclose(fptr);
}

void GrabRefString(int reference_string[NUM_REFERENCES]) {
    char fname[strlen(IN_FNAME) + strlen(".txt") + 1];
    strcpy(fname, IN_FNAME);
    strcat(fname, ".txt");
    
    FILE* fptr = fopen(fname, "r");
    char c;
    int current = 0;
    bool multi_letter = false;
    size_t idx = 0;
    while((c = fgetc(fptr)) != EOF) {
        if (c != ' ') {
            if (!current)
                current += ((int)c - 48) * 10;
            else {
                current += (int)c - 48;
                multi_letter = true;
            }
        }
        else {
            if (!multi_letter)
                reference_string[idx] = current / 10;
            else
                reference_string[idx] = current;

            multi_letter = false;
            current = 0;
            idx++;
        }
    }
    fclose(fptr);
}

char* int_to_char(int x) {
    if (x == 0)
        return "0";
    else if (x == 1)
        return "1";
    else if (x == 2)
        return "2";
    else if (x == 3)
        return "3";
    else if (x == 4)
        return "4";
    else if (x == 5)
        return "5";
    else if (x == 6)
        return "6";
    else if (x == 7)
        return "7";
    else if (x == 8)
        return "8";
    else if (x == 9)
        return "9";
    else if (x == 10)
        return "10";
    else if (x == 11)
        return "11";
    else if (x == 12)
        return "12";
    else if (x == 13)
        return "13";
    else if (x == 14)
        return "14";
    else if (x == 15)
        return "15";
}
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>


void print();

void update_curr_studying(int id, int remove);

#define MAX_NUM_STUDENTS 40
#define MIN_NUM_STUDENTS 20

#define NUM_STUDYING_POSITIONS 8

#define MIN_STUDY_TIME 5
#define MAX_STUDY_TIME 15

#define WAIT_TIME 100 * 1000

int curr_studying = 0;
int must_wait = 0;
pthread_mutex_t studying_mutex = PTHREAD_MUTEX_INITIALIZER;

// Define a struct to represent a student
typedef struct {
    int student_id;
    int isStudying;
    sem_t semaphore;
} Student;

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Define a queue to hold waiting students
Student *waiting_students[MAX_NUM_STUDENTS];
int waiting_front = 0;
int waiting_rear = 0;
int waiting_count = 0;

//array holding ids of students curr studying
int curr_studying_ids[NUM_STUDYING_POSITIONS];

// Define a lock for the waiting queue
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

void enqueue(Student* student) {
    pthread_mutex_lock(&queue_mutex);
    waiting_students[waiting_rear] = student;
    waiting_rear = (waiting_rear + 1) % MAX_NUM_STUDENTS;
    waiting_count++;
    pthread_mutex_unlock(&queue_mutex);
}

Student* dequeue() {
    pthread_mutex_lock(&queue_mutex);

    //check for empty queue
    if(waiting_count == 0){
        return NULL;
    }
    Student* student = waiting_students[waiting_front];
    waiting_front = (waiting_front + 1) % MAX_NUM_STUDENTS;
    waiting_count--;
    pthread_mutex_unlock(&queue_mutex);
    return student;
}



void *student_thread(void *arg) {
    Student *student = (Student *)arg;

    pthread_mutex_lock(&print_mutex);
    printf("Student %d was born\n", student->student_id);

    if(must_wait) {
        printf("Student %d must wait, hall is full\n", student->student_id);
        print();
    }
    pthread_mutex_unlock(&print_mutex);

    // Wait for the student's turn to enter the study room
    sem_wait(&student->semaphore);

    // Enter the study room
    student->isStudying = 1;
    pthread_mutex_lock(&studying_mutex);
    update_curr_studying(student->student_id, 0);
    curr_studying++;
    if(curr_studying == NUM_STUDYING_POSITIONS){
        must_wait = 1;
    }
    pthread_mutex_unlock(&studying_mutex);


    // Simulate studying for range seconds
    int study_time = (rand() % (MAX_STUDY_TIME - MIN_STUDY_TIME + 1)) + MIN_STUDY_TIME;
    pthread_mutex_lock(&print_mutex);
    printf("Student %d is studying for %d seconds.\n", student->student_id, study_time);
    print();
    pthread_mutex_unlock(&print_mutex);
    sleep(study_time);

    // Leave the study room
    pthread_mutex_lock(&studying_mutex);
    update_curr_studying(student->student_id, 1);
    curr_studying--;
    pthread_mutex_unlock(&studying_mutex);

    // Release the position and signal the next waiting student
    sem_post(&student->semaphore);

    pthread_mutex_lock(&print_mutex);
    printf("Student %d is done studying\n", student->student_id);
    print();
    pthread_mutex_unlock(&print_mutex);


    //if there are students waiting
    //and the hall was full when current student was studying
    //but is now empty
    //waiting students are allowed to enter
    if (waiting_count > 0 && must_wait && curr_studying == 0) {

        //signal the next max allowed students
        for (int i = 0; i < NUM_STUDYING_POSITIONS; i++) {
            Student *next_student = dequeue();
            if(next_student != NULL) {
                sem_post(&next_student->semaphore);
                usleep(WAIT_TIME);
                pthread_mutex_lock(&print_mutex);
                pthread_mutex_lock(&queue_mutex);
                print();
                pthread_mutex_unlock(&queue_mutex);
                pthread_mutex_unlock(&print_mutex);
            }
        }
    } else if(waiting_count > 0 && !must_wait && curr_studying < NUM_STUDYING_POSITIONS){
        //if there are students waiting
        //and the hall is not full
        //then signal the next student
        Student *next_student = dequeue();

        if(next_student != NULL) {
            sem_post(&next_student->semaphore);
            usleep(WAIT_TIME);

            pthread_mutex_lock(&print_mutex);
            print();
            pthread_mutex_unlock(&print_mutex);
        }

    } else if(waiting_count == 0 && curr_studying == 0) {
        exit(0);
    }

    return NULL;
}

void update_curr_studying(int id, int remove) {
    if(remove){
        for(int i = 0; i < NUM_STUDYING_POSITIONS; i++){
            if(curr_studying_ids[i] == id){
                curr_studying_ids[i] = -1;
                break;
            }
        }
        return;
    }
    for(int i = 0; i < NUM_STUDYING_POSITIONS; i++){
        if(curr_studying_ids[i] == -1){
            curr_studying_ids[i] = id;
            break;
        }
    }
}

int read_total_students() {
    int total_students;
    printf("Enter total number of students: ");
    scanf("%d", &total_students);
    while (total_students < MIN_NUM_STUDENTS || total_students > MAX_NUM_STUDENTS) {
        printf("Total number of students must be in range [%d,%d]\n", MIN_NUM_STUDENTS, MAX_NUM_STUDENTS);
        printf("Enter total number of students: ");
        scanf("%d", &total_students);
    }
    return total_students;
}

void print() {
    //print current studying students
    printf("\n");
    printf("Study Hall: ");
    for (int i = 0; i < NUM_STUDYING_POSITIONS; i++) {
        if(curr_studying_ids[i] != -1) {
            printf(" |%d| ", curr_studying_ids[i]);
        } else {
            printf(" | | ");
        }

    }
    printf("\n");

    //print waiting students
    printf("Waiting Area: ");
    if(waiting_count == 0){
        for(int i = 0; i < NUM_STUDYING_POSITIONS; i++){
            printf(" | | ");
        }
    } else {
        for (int i = waiting_front; i < waiting_rear; i++) {
            printf(" |%d| ", waiting_students[i]->student_id);
        }
    }
    printf("\n\n");

}

int main() {

    int num_students = read_total_students();

    pthread_t *students = malloc(sizeof(pthread_t) * num_students);
    Student *student_data = malloc(sizeof(Student) * num_students);

    //set studying arr to -1
    for (int i = 0; i < NUM_STUDYING_POSITIONS; i++) {
        curr_studying_ids[i] = -1;
    }

    // Initialize the random number generator
    srand((unsigned)time(NULL));

    for (int i = 0; i < num_students; i++) {
        student_data[i].student_id = i;
        student_data[i].isStudying = 0;

        sem_init(&student_data[i].semaphore, 0, 0);
    }

    // Start the first 8 students
    for (int i = 0; i < NUM_STUDYING_POSITIONS; i++) {
        student_data[i].isStudying = 1;
        sem_post(&student_data[i].semaphore);
    }

    // Create student threads
    for (int i = 0; i < num_students; i++) {
        pthread_create(&students[i], NULL, student_thread, &student_data[i]);
        if (i >= NUM_STUDYING_POSITIONS) {
            enqueue(&student_data[i]);
        } else {
            curr_studying_ids[i] = student_data[i].student_id;
        }
        usleep(WAIT_TIME);
    }

    // Wait for all threads to finish
    for (int i = 0; i < num_students; i++) {
        pthread_join(students[i], NULL);
    }

    // Destroy the semaphores
    for (int i = 0; i < num_students; i++) {
        sem_destroy(&student_data[i].semaphore);
    }

    return 0;
}


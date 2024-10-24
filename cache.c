/*
 Implementation for the Cache ADT
 Jay Choudhary




*/
#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "mem.h"
#include <assert.h>
#include "table.h"
#include "atom.h"
#include "seq.h"     
#include <time.h>   
#include <fcntl.h> 
#include <unistd.h> 
#include <time.h> 
#include <sys/stat.h>
#include "set.h"
#include <errno.h>
#define MAX_FILE_SIZE 10*1024*1024
#define OUTPUT_SUFFIX 8
#define MAX_LINE_LEN 1024
#define BUFFER_SIZE 1024
typedef struct File *File;
typedef struct Node *Node;
typedef struct LL *LL;
File get_oldest_stale(Cache c);
void evict_oldest_stale(File old_stale,Cache c);
void evict_file(File unused_file,Cache ch);
void print_lru(LL lru);
void print_files_array(Cache ch);
bool exists_in_cache(Cache ch,char* file_name);
File Cache_array_get(Cache ch,char* file_name);
void Cache_update_file(Cache ch,char* file_name,int age);
void Cache_put_vacant(Cache ch,char* file_name,int age);


struct Cache{
        int curr_size;
        int max_size;
       // int free_index;
        // Table_T table; // for quick access,hash table
        //Seq_T stale_items;
        File *files_array; 
        //Set_T retrieved_files;
        int num_stale_files;
        LL list_lru;
        Table_T map_lru;



};






struct File{
        char* content;
        char* name;
        struct timespec expiration_time;
        ssize_t bytes_read;
        bool output_file_exists;
        bool isStale;
        bool isRetrieved;
        struct timespec storage_time;
        char* output_file_name;
       // struct timespec retrieval_time;
    
};

struct Node{

        File file;
        Node next;
};
struct LL{
       
       int size;
       Node head;
       



};







File File_new(char* file_name, int age) {
    int f = open(file_name, O_RDONLY);
    if (f < 0) {
        perror("Error opening file");
        return NULL;
    }

   
    char* buffer = malloc(MAX_FILE_SIZE);
    ssize_t bytes_read = read(f, buffer, MAX_FILE_SIZE);
    if (bytes_read < 0) {
        perror("Error reading file");
        close(f);
        free(buffer);
        return NULL;
    }

    
    File new_file = malloc(sizeof(*new_file));

     new_file->name =  (char *)malloc(strlen(file_name) + 1);
     if (new_file->name == NULL) {
        perror("Failed to allocate memory for file name");
        free(new_file); // Free the File struct if name allocation fails
        return NULL;
    }
    strcpy(new_file->name, file_name);
    new_file->content = buffer;
    clock_gettime(CLOCK_REALTIME, &new_file->storage_time);
    
   
    new_file->expiration_time.tv_sec = new_file->storage_time.tv_sec + age;
    new_file->expiration_time.tv_nsec = new_file->storage_time.tv_nsec;
    
    new_file->bytes_read = bytes_read;
    new_file->output_file_exists = false;
    new_file->isStale = false;
   
    new_file->isRetrieved = false;
   

    close(f);
    assert(new_file!=NULL);
    return new_file;
}





void File_free(File f){
        FREE(f->content);
        free(f->name);
        FREE(f);
        f= NULL;
}

Node Node_new(File f,Node next){
        Node n = ALLOC(sizeof(*n));
        n->file = f;
        n->next = next;
        assert(n!=NULL);
        return n;
}

LL LL_new(Node head,int size){
        LL lru_list = ALLOC(sizeof(*lru_list));
        lru_list->head = head;
        lru_list->size = size;
        assert(lru_list!=NULL);
        return lru_list;
}

void Node_free(Node n){
        FREE(n);
        n = NULL;
}
void LL_free(LL lru_list){
        FREE(lru_list);
}

void insert_lru(LL lru,File f,Cache ch){

        assert(Table_get(ch->map_lru,(void*)f)==NULL);
        Node new_node = Node_new(f,lru->head);
        lru->head = new_node;
        lru->size++;
        Table_put(ch->map_lru,f,new_node);


}

void update_lru(LL lru,File f,Cache ch){
        assert(Table_get(ch->map_lru,(void*)f)!=NULL);
        //if already at head
        if(lru->head->file == f){
                return;
        }
        Node new_head = Table_get(ch->map_lru,(void*)f);
        Node prev = lru->head;
        while(prev->next != new_head){
                prev = prev->next;
        }
        Node new_next = new_head->next;
        new_head->next = lru->head;
        prev->next = new_next;

        //printf("LRU updated\n");
         //print_lru(ch->list_lru);
        return;

}
void update_LRU(Cache ch,char *file_name){

        File f = Cache_array_get(ch,file_name);
        assert(f);
        update_lru(ch->list_lru,f,ch);
        return;


}

void evict_stale_from_lru(File stale_file,Cache ch){

        assert(stale_file->isStale);
        assert((File)Table_get(ch->map_lru,(void*)stale_file)!=NULL);
         Node evict_node = Table_get(ch->map_lru,(void*)stale_file);
          assert(evict_node!=NULL);
          LL lru = ch->list_lru;
        Node prev = lru->head;
        if(evict_node==lru->head){
                lru->head = lru->head->next;
                lru->size--;
        }
        else{
                
                while(prev->next != evict_node){
                        prev = prev->next;
                       
                }
                Node new_next = evict_node->next;
                prev->next = new_next;
                lru->size--;

        }
          Table_remove(ch->map_lru,(void*)stale_file);
          return;



}

void evict_from_lru(LL lru,File f,Cache ch){

        assert(lru->size > 0);
        Node evict_node = Table_get(ch->map_lru,(void*)f);
        assert(evict_node!=NULL);
        Node prev = lru->head;
        if(evict_node==lru->head){
                lru->head = lru->head->next;
                lru->size--;
        }
        else{
                
                while(prev->next != evict_node){
                        prev = prev->next;
                       
                }
                Node new_next = evict_node->next;
                prev->next = new_next;
                lru->size--;

        }
        Table_remove(ch->map_lru,(void*)f);
       
        bool found = false;
        for(int i =0;i<ch->max_size;i++){
                File f1 = ch->files_array[i];
                if(f1==f){
                        ch->files_array[i]=NULL;
                        found = true;
                }
        }

        assert(found);
        printf("File named %s evicted from lru and cache\n",f->name);

        assert(f->isRetrieved);

           
        if (unlink(f->output_file_name) == 0) {
            printf("Output file '%s' removed successfully.\n", f->output_file_name);
        } else {
            perror("Error removing output file");
        }
    

        File_free(f);
        print_lru(ch->list_lru);
        ch->curr_size--;









         
        return;


}

File get_lru_file(Cache ch){
        assert(ch->num_stale_files == 0);
        LL lru = ch->list_lru;
        assert(lru->size > 0);
        Node curr = lru->head;
        assert(curr!=NULL);
        while(curr->next != NULL){

                curr = curr->next;
                
        }
        assert(curr!=NULL);
        File lru_file = curr->file;
        assert(lru_file!=NULL);
        return lru_file;
}

void print_lru(LL lru){

        if(lru->size == 0){
                printf("LRU Empty\n");
        }
        else{
                printf("printing LRU\n");
                Node curr = lru->head;
                while(curr!=NULL){
                        File f = curr->file;
                        printf("%s ",f->name);
                        curr = curr->next;
                }
                printf("\n");
        }
}



Cache Cache_new(int num_items){

        Cache c = ALLOC(sizeof(*c));
        assert(c!=NULL);
        c->curr_size = 0;
        c->max_size = num_items;
       // c->free_index = 0;
        //c->table = Table_new(c->max_size,cmp,NULL);
        c->files_array = ALLOC(sizeof(File) * c->max_size);
        for(int i = 0;i<c->max_size;i++){
                c->files_array[i]=0;
        }
       //c->stale_items = Seq_new(c->max_size);
       c->num_stale_files = 0;
       c->list_lru = LL_new(NULL,0);
       c->map_lru = Table_new(c->max_size,NULL,NULL);
        return c;


}


void check_stale_files(Cache c){

         struct timespec current_time;
         clock_gettime(CLOCK_REALTIME, &current_time);
      
        if(c->curr_size==0){
                return;
        }
        assert(c!=NULL);
        for(int i = 0 ; i<c->max_size; i++){

                File f = c->files_array[i];
                if(f!=NULL){

                
                if(current_time.tv_sec > f->expiration_time.tv_sec ||
                         (current_time.tv_sec == f->expiration_time.tv_sec && current_time.tv_nsec > f->expiration_time.tv_nsec)){

                       
                               f->isStale = true;
                               c->num_stale_files++;
                               if(f->isRetrieved){

                                                evict_stale_from_lru(f,c);

                               }
                              
                        

                }}
        }

}

void Cache_update_file(Cache ch,char *file_name,int age){

        int f = open(file_name, O_RDONLY);
         if (f < 0) {
        perror("Error opening file");
        return;
    }
     check_stale_files(ch);
   
   
    char* buffer = malloc(MAX_FILE_SIZE);
    ssize_t bytes_read = read(f, buffer, MAX_FILE_SIZE);
    if (bytes_read < 0) {
        perror("Error reading file");
        close(f);
        free(buffer);
        return ;
    }

    File old_file = Cache_array_get(ch,file_name);
    assert(old_file!=NULL);
    free(old_file->content);
    old_file->content = buffer;
    clock_gettime(CLOCK_REALTIME, &old_file->storage_time);
    
   
    old_file->expiration_time.tv_sec = old_file->storage_time.tv_sec + age;
    old_file->expiration_time.tv_nsec = old_file->storage_time.tv_nsec;
   
    old_file->bytes_read = bytes_read;

    if(old_file->isStale){
    old_file->isStale = false;
   
    
                ch->num_stale_files--;
               
          
    }
    update_LRU(ch,file_name);
              return;


}

void Cache_put(Cache ch,char* file_name,int age)
{

       //If it is in cache
      // printf("Requesting to put file named %s\n",file_name);
      // printf("Current cache size is %d\n",ch->curr_size);
      // printf("Current lru is \n");
      // print_lru(ch->list_lru);
       //print_files_array(ch);


       //CASE 1
       if(exists_in_cache(ch,file_name)){

        Cache_update_file(ch,file_name,age);



        return;
    }

   
    

        else if(ch->curr_size==ch->max_size){

        check_stale_files(ch);



        //If there is a stale_file
        if(ch->num_stale_files>0){
                File evict = get_oldest_stale(ch);
                assert(evict!=NULL);
                evict_oldest_stale(evict,ch);
                Cache_put_vacant(ch,file_name,age);
                return;

                
        }
        else{  // no stale files - either unused or retrieved 
        
        //checking if unused file exists
        bool unused_file_exists = false;
        struct timespec old_time ;
        clock_gettime(CLOCK_REALTIME, &old_time);
        File unused_file = NULL;
        for(int i = 0;i<ch->max_size;i++){
                File f1 = ch->files_array[i];
                if(f1){
                        if(f1->isRetrieved == false){
                                unused_file_exists = true;
                                if(f1->storage_time.tv_sec < old_time.tv_sec || ((f1->storage_time.tv_sec == old_time.tv_sec)&&(f1->storage_time.tv_nsec<old_time.tv_nsec))){
                                        unused_file = f1;
                                        old_time = f1->storage_time;
                                }
                        }
                }
        }




        if(unused_file_exists){
                assert(unused_file!=NULL);
                evict_file(unused_file,ch);
                Cache_put_vacant(ch,file_name,age);
                return;
        }


        else{


                //Remove from lru
                assert(ch->list_lru->size>0);
                File evict = get_lru_file(ch);
                assert(evict!=NULL);
                evict_from_lru(ch->list_lru,evict,ch);
                Cache_put_vacant(ch,file_name,age);
                return;

        } 

        }
       



       }



       else{ 
         assert(ch->curr_size<ch->max_size);
        Cache_put_vacant(ch,file_name,age);
        return;



       }

}


       


 void Cache_put_vacant(Cache ch,char* file_name,int age){
        File new_file = File_new(file_name,age);
        assert(new_file!=NULL);

        assert(ch->curr_size != ch->max_size);
        //void* v = Table_put(ch->table,(void*)new_file->name,(void*)new_file);
       // assert((File)v == NULL);
       
        bool found_spot = false;

        for(int i = 0 ; i<ch->max_size;i++){
                File spot = ch->files_array[i];
                if(spot == NULL){
                        found_spot = true;
                        ch->files_array[i] = new_file;
                        break;
                }
        
        }

        assert(found_spot);
         printf("Inserted file %s in vacant spot\n",new_file->name);

        ch->curr_size++;
        new_file->isRetrieved = true;
        insert_lru(ch->list_lru,new_file,ch);

        return;






       }



           





void evict_file(File unused_file,Cache ch){
        bool found = false;
        for(int i = 0;i<ch->max_size;i++){
                File f = ch->files_array[i];
                if(f==unused_file){
                        found = true;
                        ch->files_array[i] = NULL;
                }
        }
        assert(found);

         if (unused_file->isRetrieved) {  
        if (unlink(unused_file->output_file_name) == 0) {
            printf("Output file '%s' removed successfully.\n", unused_file->output_file_name);
        } else {
            perror("Error removing output file");
        }
    }

    assert(!unused_file->isStale);
    File_free(unused_file);
    unused_file = NULL;
       
        ch->curr_size--;
        return;
}

void Cache_free(Cache* ch){

       // Table_free(&((*ch)->table));
        
        for(int i = 0;i<((*ch)->curr_size);i++){
                File f = (*ch)->files_array[i];
                if(f!=NULL){
                        File_free(f);
                }
        }
        Table_free(&((*ch)->map_lru));

        //Seq_free(&((*ch)->stale_items));
        FREE(((*ch)->files_array));

        
        FREE(*ch);
        *ch = NULL;
        
}











File get_oldest_stale(Cache ch){

        if(ch->curr_size == 0){
                return NULL;
        }
        assert(ch->num_stale_files>0);
        
      
        struct timespec old_time ;
        clock_gettime(CLOCK_REALTIME, &old_time);
        File old_stale = NULL;
        for(int i = 0;i<ch->max_size;i++){
                File f1 = ch->files_array[i];
                if(f1){
                        if(f1->isStale){
                                
                                if(f1->storage_time.tv_sec < old_time.tv_sec || ((f1->storage_time.tv_sec == old_time.tv_sec)&&(f1->storage_time.tv_nsec<old_time.tv_nsec))){
                                        old_stale = f1;
                                        old_time = f1->storage_time;
                                }
                        }
                }
        }
        return old_stale;
}




void evict_oldest_stale(File old_stale,Cache c){

               
               bool found = false;
                for(int i=0;i<c->max_size;i++){
                        if(c->files_array[i]==old_stale){
                                c->files_array[i] = NULL;
                                found = true;
                                break;
                        }
                }
                assert(found);


                 if (old_stale->isRetrieved) {  
        if (unlink(old_stale->output_file_name) == 0) {
            printf("Output file '%s' removed successfully.\n", old_stale->output_file_name);
        } else {
            perror("Error removing output file");
        }
    }
                
               
                File_free(old_stale);
                old_stale=NULL;
                c->num_stale_files--;
                c->curr_size--;
}






void write_to_output_file(File f) {
    char *file_name = f->name;
    char *buffer = f->content;
    ssize_t buffer_size = f->bytes_read;

    size_t file_name_len = strlen(file_name);
    char* dot_position = strrchr(file_name, '.');  

    size_t base_name_len;
    if (dot_position != NULL) {
        base_name_len = dot_position - file_name;  
    } else {
        base_name_len = file_name_len;  
    }

    
    char* output_file_name = (char*)malloc(base_name_len + OUTPUT_SUFFIX + (dot_position ? strlen(dot_position) : 0) + 1);
    if (output_file_name == NULL) {
        perror("Memory allocation error");
        return;
    }

    memcpy(output_file_name, file_name, base_name_len);
    memcpy(output_file_name + base_name_len, "_output", OUTPUT_SUFFIX - 1);

   
    if (dot_position != NULL) {
        strcpy(output_file_name + base_name_len + OUTPUT_SUFFIX - 1, dot_position);
    } else {
        output_file_name[base_name_len + OUTPUT_SUFFIX - 1] = '\0';  // Null-terminate if no extension
    }

    int fd = open(output_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Error opening output file");
        free(output_file_name);
        return;
    }

    
    ssize_t bytes_written = write(fd, buffer, buffer_size);
    if (bytes_written < 0) {
        perror("Error writing to output file");
    } else if (bytes_written != buffer_size) {
        write(STDERR_FILENO, "Warning: Not all bytes written to output file\n", 46);
    }

    
    if (close(fd) < 0) {
        perror("Error closing output file");
    }

    f->output_file_name = output_file_name;

    //free(output_file_name);
}


void print_files_array(Cache ch){
        for(int i = 0;i<ch->max_size;i++){
                File f = ch->files_array[i];
                if(f){
                        printf("%s ",f->name);
                }
                else{
                        printf("NULL ");
                }
        }
        printf("\n");
}


bool exists_in_cache(Cache ch,char* file_name){
        for(int i=0;i<ch->max_size;i++){
                File f = ch->files_array[i];
                if(f){
                        if(strcmp(file_name,f->name)==0){
                                return true;
                        }
                }
        }
        return false;
}

File Cache_array_get(Cache ch,char* file_name){

        for(int i=0;i<ch->max_size;i++){
                File f = ch->files_array[i];
                if(f){
                        if(strcmp(file_name,f->name)==0){
                                return f;
                        }
                }
        }
        return NULL;

}

void Cache_get(Cache ch,char *file_name){

        
       // check_stale_files(ch);
       //  printf("Requesting to get file named %s\n",file_name);
      // printf("Current cache size is %d\n",ch->curr_size);
      // printf("Current lru is \n");
      // print_lru(ch->list_lru);
      // print_files_array(ch);
       bool exists = exists_in_cache(ch,file_name);
       if(exists){
         printf("%s is in cache\n",file_name);


       }
       else{
        printf("%s not in cache\n",file_name);
        return;
      
       }
       

        File output_file = Cache_array_get(ch,file_name);

        if(!output_file->isStale){ 
        //printf("File name is %s\n",output_file->name);
        // printf("File content is %s\n",output_file->content);

        

        write_to_output_file(output_file);
         if(!output_file->isRetrieved){
         
         insert_lru(ch->list_lru,output_file,ch);

         
         output_file->isRetrieved = true;
        // output_file->retrieval_time = time(NULL);
         }
         else{
                update_lru(ch->list_lru,output_file,ch);
         }

        return;
        }
        

        // File is Stale and being retrieved
        int age = output_file->expiration_time.tv_sec - output_file->storage_time.tv_sec;
        File new_output_file = File_new(file_name,age);

        //Remove previous file from Cache - Table,Sequence , Array , decrease stale_file_count
         evict_oldest_stale(output_file,ch);
         //Table_put(ch->table,file_name,(void*)new_output_file);
         //Put in array

         bool inarray = false;
         for(int i = 0 ; i<ch->max_size;i++){
                if(ch->files_array[i]==NULL){
                        ch->files_array[i]=new_output_file;
                        inarray = true;
                        break;
                }
         }
         assert(inarray == true);
         ch->curr_size++;

        // printf("File content is %s\n",new_output_file->content);
         //printf("File name is %s\n",new_output_file->name);

        


        write_to_output_file(new_output_file);
        
        //new_output_file->output_file_exists = true;
        new_output_file->isRetrieved = true;
        insert_lru(ch->list_lru,new_output_file,ch);
       // new_output_file->retrieval_time = time(NULL);
        return;



      
}

bool isStale(Cache ch,char *file_name){
        assert(exists_in_cache(ch,file_name));
        check_stale_files(ch);
        File f = Cache_array_get(ch,file_name);
        assert(f);
        return f->isStale;

}

char* return_file_content(Cache ch,char *file_name){

        File f = Cache_array_get(ch,file_name);
        assert(f);
        assert(!f->isStale);
        return f->content;
}
int get_file_age(Cache ch,char *file_name){

        File f = Cache_array_get(ch,file_name);
        assert(f);
        assert(!f->isStale);
         struct timespec current_time;
         clock_gettime(CLOCK_REALTIME, &current_time);
         return (f->expiration_time.tv_sec - current_time.tv_sec);

}




// void process_commands_file(Cache cache, const char *filename) {
//     int fd = open(filename, O_RDONLY);
//     if (fd < 0) {
//         perror("Error opening commands file");
//         return;
//     }

//     char line[MAX_LINE_LEN];
//     int line_index = 0;
//     ssize_t bytes_read;
//     bool is_last_line = false;

//     // Read each character and process line by line
//     while ((bytes_read = read(fd, line + line_index, 1)) > 0 || line_index > 0) {
//         if (bytes_read == 0) {
//             // Handle the case where the file ends without a newline
//             is_last_line = true;
//         }

//         if (line[line_index] == '\n' || is_last_line) {
//             line[line_index] = '\0';  // Null-terminate the line

//             // Check if it's a PUT command
//             if (strncmp(line, "PUT: ", 5) == 0) {
//                 char *filename_ptr = line + 5;
//                 char *max_age_ptr = strstr(line, "\\MAX-AGE: ");
//                 if (max_age_ptr) {
//                     *max_age_ptr = '\0';  // Null-terminate the filename part
//                     max_age_ptr += strlen("\\MAX-AGE: ");

//                     // Parse max-age as an integer
//                     int max_age = atoi(max_age_ptr);
//                     if (max_age > 0) {
//                         printf("PUT command: filename=%s, max_age=%d\n", filename_ptr, max_age);

//                         // Remove any newline character at the end of the filename
//                         size_t len = strlen(filename_ptr);
//                         if (len > 0 && filename_ptr[len - 1] == '\n') {
//                             filename_ptr[len - 1] = '\0';
//                         }

//                         // Make a null-terminated copy of the filename before passing it to Cache_put
//                         char *filename_copy = strdup(filename_ptr);
//                         if (filename_copy) {
//                             Cache_put(cache, filename_copy, max_age);
//                             free(filename_copy);  // Free the allocated copy after use
//                         } else {
//                             fprintf(stderr, "Error allocating memory for filename\n");
//                         }
//                     } else {
//                         fprintf(stderr, "Error: Invalid MAX-AGE value\n");
//                     }
//                 } else {
//                     fprintf(stderr, "Error: Missing MAX-AGE in PUT command\n");
//                 }
//             }

//             // Check if it's a GET command
//             if (strncmp(line, "GET: ", 5) == 0) {
//                 char *filename_ptr = line + 5;

//                 // Remove the newline character at the end of the filename if it exists
//                 size_t len = strlen(filename_ptr);
//                 if (len > 0 && filename_ptr[len - 1] == '\n') {
//                     filename_ptr[len - 1] = '\0';  // Replace newline with null terminator
//                 }

//                 printf("GET command: filename=%s\n", filename_ptr);

//                 // Make a null-terminated copy of the filename before passing it to Cache_get
//                 char *filename_copy = strdup(filename_ptr);
//                 if (filename_copy) {
//                     Cache_get(cache, filename_copy);  // Call Cache_get with the correct filename
//                     free(filename_copy);  // Free the allocated copy after use
//                 } else {
//                     fprintf(stderr, "Error allocating memory for filename\n");
//                 }
//             }

//             // Reset index for the next line
//             line_index = 0;
//             if (is_last_line) {
//                 break;
//             }
//         } else {
//             if (line_index < MAX_LINE_LEN - 1) {
//                 line_index++;
//             } else {
//                 fprintf(stderr, "Error: Line exceeds maximum length\n");
//                 close(fd);
//                 return;
//             }
//         }
//     }

//     if (bytes_read < 0) {
//         perror("Error reading command file");
//     }

//     close(fd);
// }




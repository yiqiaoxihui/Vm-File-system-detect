#include "base.h"
#include "include/public.h"
int main()
{
    //mtrace();

//    char **image_abspath;
//    char **image_id;
//    int images_count=0;
//    int i;
//    int thread_create;
//    /******指针空间必须在主函数中分配*******/
//    image_abspath=malloc((MAX_OVERLAY_IMAGES+1)*sizeof(char *));
//    image_id=malloc((MAX_OVERLAY_IMAGES+1)*sizeof(char *));
//    /*********************************read the image path***************************************/
//    if(read_host_image_name(image_abspath,image_id)<=0){
//        printf("\n read image abspath failed!");
//        return 0;
//    }
//    for(i=0;image_abspath[i];i++){
//            images_count++;
//    }
//    printf("\nMMMMMM i:%d,image count:%dMMMMMM",i,images_count);
//    read_image_thread=malloc(images_count*sizeof(pthread_t));
//    threadVar=malloc(images_count*sizeof(struct ThreadVar));
//    for(i=0;i<images_count;i++){
//        //threadVar[434].image_id=image_abspath[i];
//        printf("\nMMMMMMoverlay:%s,id:%sMMMMMM",image_abspath[i],image_id[i]);
//        threadVar[i].image_id=image_id[i];
//        threadVar[i].image_path=image_abspath[i];
//        thread_create=pthread_create(&read_image_thread[i],NULL,multi_read_image_file,(void *)&threadVar[i]);
//        if(thread_create){
//            printf(stderr,"MMMMMM\nError -%d pthread_create() return code: %dMMMMMM",i,thread_create);
//            return 0;
//        }
//    }
//    for(i=0;i<images_count;i++){
//        pthread_join(read_image_thread[i],NULL);
//    }
//
//    free(image_abspath);
//    free(image_id);
//    free(threadVar);
//    free(read_image_thread);

    statistics_proportion();
    return 0;
}


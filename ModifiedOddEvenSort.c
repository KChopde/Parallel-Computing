#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <stdbool.h>
#include <time.h>
// mpirun -n 8 ./programname 10

  /* The IncOrder function that is called by qsort is defined as follows */
int IncOrder(const void *e1, const void *e2) {
        return (*((int *)e1) - *((int *)e2));
    }

int random_inrange(){
    return random() % 129;
}



int main(int argc, char *argv[]) {
    int n; /* The total number of elements to be sorted */
    int npes; /* The total number of processes */
    int myrank; /* The rank of the calling process */
    int nlocal; /* The local number of elements, and the array that stores them */
    int *elmnts; /* The array that stores the local elements */
    int *relmnts; /* The array that stores the received elements */
    int *wspace; /* Working space during the compare-split operation */
    int *inputarr; /* Array we want to sort of size n */
    int *sorted_arr;
    int oddrank; /* The rank of the process during odd-phase communication */
    int evenrank; /* The rank of the process during even-phase communication */
    int i;
    MPI_Status status;
    double startTime, endTime;
    bool done = false;
    

    // printf("MPI will init\n");
    /* Initialize MPI and get system information */
    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &npes);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    n = atoi(argv[1]);

    nlocal = n / npes; /* Compute the number of elements to be stored locally. */

    elmnts = (int *)malloc(nlocal * sizeof(int)); //local buffer
    relmnts = (int *)malloc(nlocal * sizeof(int)); //receving buffer
    wspace = (int *)malloc(nlocal * sizeof(int)); // for compare-split operation


    srandom(time(NULL)); // avoid generating same sequence of random numbers
    if (myrank == 0){
        inputarr = (int *)malloc(n * sizeof(int));

        sorted_arr = (int *)malloc(n * sizeof(int)); // allocate sorted array here as well.

        // randomly assign integers in the range[0,128] for the input array
        for (i = 0; i < n; i++){
            inputarr[i] = random_inrange();
        }
    }

    // start timer
    startTime = MPI_Wtime();


    /* Determine the rank of the processors that myrank needs to communicate during
    the odd and even phases of the algorithm */
    if (myrank % 2 == 0) {
        oddrank = myrank - 1;
        evenrank = myrank + 1;
    } else {
        oddrank = myrank + 1;
        evenrank = myrank - 1;
    }

    /* Set the ranks of the processors at the end of the linear */
    if (oddrank == -1 || oddrank == npes)
        oddrank = MPI_PROC_NULL;
    if (evenrank == -1 || evenrank == npes)
        evenrank = MPI_PROC_NULL;

    // Process with rank 0 sends data to all other processes
    MPI_Scatter(inputarr, nlocal, MPI_INT, elmnts, nlocal, MPI_INT, 0, MPI_COMM_WORLD);

    /* Sort the local elements using the built-in quicksort routine */
    qsort(elmnts, nlocal, sizeof(int), IncOrder);

    int sameSubarray = 0;
    int  phase = 0; // phase
    int complete = 0;
    int allTrue= 0;
 
        while(!allTrue){
            // odd even iterations

            if (phase %2 == 1) /* Odd phase */{
                //send data elmnts to process number oddrank and receive relmnts from it
                MPI_Sendrecv(elmnts, nlocal, MPI_INT, oddrank, 1, relmnts, 
                nlocal, MPI_INT, oddrank, 1, MPI_COMM_WORLD, &status);
            }
            else /* Even phase */
            {
                //send data elmnts to process number evenrank and receive relmnts from it
                MPI_Sendrecv(elmnts, nlocal, MPI_INT, evenrank, 1, relmnts, 
                nlocal, MPI_INT, evenrank, 1, MPI_COMM_WORLD, &status); 
            }

            phase++;

            // Compare-split operation

            for(i=0;i<nlocal;i++){
                wspace[i]=elmnts[i];
            }

            if(status.MPI_SOURCE==MPI_PROC_NULL)	;  // we can skip compare split if the rank is not receiving or sending anything
            else if(myrank<status.MPI_SOURCE){
                //keepsmall
                int i,j,k;
                for(i=j=k=0;k<nlocal;k++){
                    if(j==nlocal||(i<nlocal && wspace[i]<relmnts[j]))
                        elmnts[k]=wspace[i++];
                    else
                        elmnts[k]=relmnts[j++];
                }
            }
            else{
                // keep larger
                int i,j,k;
                for(i=j=k=nlocal-1;k>=0;k--){
                    if(j==-1||(i>=0 && wspace[i]>=relmnts[j]))
                        elmnts[k]=wspace[i--];
                    else
                        elmnts[k]=relmnts[j--];
                }
            }

            i = 0;
            sameSubarray = 0;

            // check if our local elements were the same as that after compare split operation
            // wspace is elmnts array before compare-split
            for (i =0; i< nlocal; i++){
                if (elmnts[i] != wspace[i]){
                    sameSubarray += 1;
                }
            }

            if(sameSubarray == 0)
            {
                complete = 1; // locally the array is same as previous phase
            }
            else{
                complete =0; // locally the array has changed
            }

            // All-reduce multiplies complete for all the ranks and store the value in alldone
            int alldone;
            MPI_Allreduce(&complete, &alldone, 1, MPI_INT, MPI_PROD, MPI_COMM_WORLD);

            allTrue = alldone; // set alldone to allTrue. when allTrue is 1, while loop will terminate

            MPI_Bcast(&allTrue, 1, MPI_INT, 0, MPI_COMM_WORLD); // Broadcast allTrue from rank 0 to all other ranks.

       }
        
    
    MPI_Barrier(MPI_COMM_WORLD); // making sure all the processes are synchronized before Gathering the data


    /* Gather the sorted subarrays to process 0 */
    MPI_Gather(elmnts, nlocal, MPI_INT, sorted_arr, nlocal, MPI_INT, 0, MPI_COMM_WORLD);

    endTime = MPI_Wtime();

    double startTime_min, endTime_max;
    MPI_Reduce(&startTime, &startTime_min, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD); // getting the earliest start time
    MPI_Reduce(&endTime, &endTime_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD); // getting the latest end time

    
    if(myrank == 0){
        printf("Maximum Execution Time: %f seconds\n", endTime_max - startTime_min, myrank);
        fflush(stdout);
    }
    

    if (myrank == 0) {
        /* Write the sorted array to a file */
        FILE *resultFile = fopen("unsorted_arr.txt", "w");
        if (resultFile == NULL) {
            fprintf(stderr, "Error: Unable to open result.txt for writing.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        for (i = 0; i < n; i++) {
            fprintf(resultFile, "%d\n", inputarr[i]);
        }

        fclose(resultFile);

        resultFile = fopen("result.txt", "w");
        if (resultFile == NULL) {
            fprintf(stderr, "Error: Unable to open result.txt for writing.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        for (i = 0; i < n; i++) {
            fprintf(resultFile, "%d\n", sorted_arr[i]);
        }

        fclose(resultFile);
    }

    free(elmnts);
    free(relmnts);
    free(wspace);
    if (myrank == 0) free(inputarr);
    if (myrank == 0) free(sorted_arr);
    MPI_Finalize();
   
 }





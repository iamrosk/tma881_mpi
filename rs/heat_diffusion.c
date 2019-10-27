#include "heat_diffusion.h"
#include <mpi.h>
#include <stddef.h> // offsetof() macro.

int main(int argc, char* argv[])
{
  MPI_Init(&argc, &argv);

  int nmb_mpi_proc, mpi_rank;
  MPI_Comm_size(MPI_COMM_WORLD, &nmb_mpi_proc);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

  // Declare variables for all processes (to skip declarations in every scope):
  double conductivity;
  int niter, width, height, total_size;
  struct input_struct{
    double c;
    int    n, w, h;
  } inputs;
  double * grid_init; // pointer to memory with initial temperatures.

 if (mpi_rank == 0) // master process:
   {
      char* ptr1 = NULL;
      char* ptr2 = NULL;
      if (argc == 3 ) {
        for ( int ix = 1; ix < argc; ++ix ) {
          ptr1 = strchr(argv[ix], 'n' );
          ptr2 = strchr(argv[ix], 'd' );
          if ( ptr1 ){
            niter = strtol(++ptr1, NULL, 10);
          } else if ( ptr2 ) {
            conductivity = atof(++ptr2);
          }
        }
      } else {
        printf("Invalid number of arguments. Correct syntax is: heat_diffusion -n#numberOfTimeSteps4 -d#diffusionConstant\n");
        exit(0);
      }
      
      char line[80];
      FILE *input = fopen("diffusion", "r");
      int i=0, j=0;
      double t=0.;
      if ( input == NULL ) {
        perror("Error opening file");
        exit(0);
      }
      //read the first line
      fgets( line, sizeof(line), input);
      sscanf(line, "%d %d", &width, &height);
      total_size = width * height;
      //store initial values (note: row major order)
      grid_init = (double*) malloc( sizeof(double) * total_size);
      for (int ix = 0; ix < total_size; ++ix) {
        grid_init[ix] = 0;
      }
      //read the rest
      while ( fgets(line, sizeof(line), input) != NULL) {
        sscanf(line, "%d %d %lf", &j, &i, &t);
        grid_init[i*width + j] = t;
      }
      fclose(input);

      inputs.c = conductivity;
      inputs.n = niter;
      inputs.w = width;
      inputs.h = height;
   }
 
 // Create custom MPI data type for the structure inputs:
 MPI_Datatype mpi_double_3int;
 int block_lengths[4] = {1,1,1,1}; // 1 double and 3 single integers in structure. 
 MPI_Aint offsets[4];
 offsets[0] = offsetof( struct input_struct, c );
 offsets[1] = offsetof( struct input_struct, n );
 offsets[2] = offsetof( struct input_struct, w );
 offsets[3] = offsetof( struct input_struct, h );
 MPI_Datatype types[4] = {MPI_DOUBLE, MPI_INT, MPI_INT, MPI_INT};
 MPI_Type_create_struct( 4, // number of elements in input structure.
                         block_lengths, offsets, types,
                         &mpi_double_3int);
 MPI_Type_commit(&mpi_double_3int);
 
 // Send inputs to all processes: 
 // All the processes have to execute this statement.
 MPI_Bcast( &inputs, 1, // 1 structure.
            mpi_double_3int,
            0, // master sends out inputs.
            MPI_COMM_WORLD );

 if (mpi_rank == 0) // master process
   {
     // distribute the work (each process recieves only its part of array):
     int row_start, row_end;
     for ( int mpi_proc = 1; // master (0) already has access to its part.
           mpi_proc < nmb_mpi_proc;
           ++mpi_proc ){
       // to process row i rows i-1 and i+1 are needed:
       row_start = mpi_proc * height / nmb_mpi_proc - 1; 
       row_end = (mpi_proc + 1)* height / nmb_mpi_proc;

       MPI_Send( grid_init + row_start * width, // address of send buffer.
                 (row_end - row_start) * width, // number of elements.
                 MPI_DOUBLE, mpi_proc, // destination.
                 mpi_proc, MPI_COMM_WORLD );
     }
     

     // Update the boundary shared with other process
   }
 else // worker processes.
   {
     // receive the part of array to work on:
     conductivity = inputs.c;
     niter = inputs.n;
     width = inputs.w;
     height = inputs.h;
     total_size = width * height;

     int row_start = mpi_rank * height / nmb_mpi_proc - 1; 
     int row_end =  (mpi_rank + 1)* height / nmb_mpi_proc;
     double * grid_local = (double *) malloc( ( row_end - row_start ) *
                                              width * sizeof(double) );
     MPI_Status status;
     MPI_Recv( grid_local, // receiving buffer
               ( row_end - row_start ) * width, // number of elements
               MPI_DOUBLE, 0, // sending process
               mpi_rank, MPI_COMM_WORLD, &status);

     free(grid_local);
   }


 /* int const sub_size = (height - 1) / nmb_mpi_proc + 1; // split row-wise. */
  

 /*  double hij, hijW, hijE, hijS, hijN; */
 /*  for ( int n = 0; n < niter; ++n ){ */
 /*    for (int ix = row_start; ix < row_end; ++ix){ */
 /*      for (int jx = 0; jx < width; ++jx){ */
 /*        hij = grid_init[ ix * width +jx ]; */
 /*        hijW = ( jx-1 >= 0 ? grid_init[ ix * width + jx-1 ] : 0. ); */
 /*        hijE = ( jx+1 < width ? grid_init[ ix * width + jx+1 ] : 0.); */
 /*        hijS = ( ix+1 < height ? grid_init[ (ix+1) * width + jx ] : 0.); */
 /*        hijN = ( ix-1 >= 0 ? grid_init[ (ix-1) * width + jx ] : 0.); */

 /*        nextTimeEntries[ ix * width + jx] = hij + */
 /*          c * ( 0.25 * ( hijW + hijE + hijS + hijN ) - hij ); */
 /*      } */
 /*    } */
 /*    for ( int i =0; i < nmb_mpi_proc; ++i){ */
 /*    MPI_Bcast( nextTimeEntries + row_start*width, (row_end-row_start)*width, */
 /*               MPI_DOUBLE, i, MPI_COMM_WORLD); */
 /*    } */


 /*    if ( n == niter - 1 ) break; // don't swap at the last step */
 /*    // Swap new and old arrays: */
 /*    dummy_ptr = grid_init; */
 /*    grid_init = nextTimeEntries; */
 /*    nextTimeEntries = dummy_ptr; */
 /*  } */

  //////////////////////////// Compute average /////////////////////////////////
  /* double loc_sum = 0; */
  /* for ( int ix = row_start; ix < row_end; ++ix ) { */
  /*   for ( int jx = 0; jx < width; ++jx ) { */
  /*     loc_sum += nextTimeEntries[ ix * width + jx ]; */
  /*   } */
  /* } */
  /* double total_sum; */
  /* MPI_Reduce( &loc_sum, &total_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD ); */
  /* double avg = total_sum / total_size; */


  //////////////////////// Compute average of abs diff /////////////////////////
  /* loc_sum = 0; */
  /* for ( int ix = row_start; ix < row_end; ++ix ) { */
  /*   for ( int jx = 0; jx < width; ++jx ) { */
  /*     nextTimeEntries[ ix * width + jx ] -= avg; */
  /*     loc_sum += ( nextTimeEntries[ ix * width + jx ] < 0. ? -1. * nextTimeEntries[ ix * width + jx ] : nextTimeEntries[ ix * width + jx ] ); */
  /*   } */
  /* } */
  /* MPI_Reduce( &loc_sum, &total_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD ); */
  /* double avg_diff = total_sum / total_size; */

  //////////////////////////////// Processing //////////////////////////////////
  /* int scatter_root = 0; // master */
  /* int const sub_size = (height - 1) / nmb_mpi_proc + 1; // split row-wise. */

  /* int * len_array = (int *) malloc( nmb_mpi_proc * sizeof(int) ); */
  /* int * pos_array = (int *) malloc( nmb_mpi_proc * sizeof(int) ); */
  /* for ( int jx=0, pos=0;  jx < nmb_mpi_proc;  ++jx, pos += sub_size-2 ) { */
  /*   len_array[jx] = sub_size < height - pos ? (sub_size+2)*width : (height - pos)*width; */
  /*   pos_array[jx] = pos * width; */
  /*   if (mpi_rank==0) */
  /*     printf("len pos %d %d\n", len_array[jx], pos_array[jx]); */
  /* } */

  /* int * len_gather_array = (int *) malloc( nmb_mpi_proc * sizeof(int) ); */
  /* int * pos_gather_array = (int *) malloc( nmb_mpi_proc * sizeof(int) ); */
  /* for ( int jx=0, pos=0;  jx < nmb_mpi_proc;  ++jx, pos += sub_size ) { */
  /*   len_gather_array[jx] = sub_size < height - pos ? sub_size*width : (height - pos)*width; */
  /*   pos_gather_array[jx] = pos * width; */
  /* } */

  /* double * temperature_sub = (double *) malloc( (sub_size + (int)2) * width * sizeof(double) ); */
  /* double * temperature_sub_new = (double *) malloc( (sub_size + (int)2) * width * sizeof(double) ); */
  /* double hij, hijW, hijE, hijS, hijN; */
  /* int shift; */
  /* for ( int n = 0; n < niter; ++n ){ */
  /*   MPI_Scatterv( grid_init, len_array, pos_array, MPI_DOUBLE, */
  /*                 temperature_sub, (sub_size+2)*width, MPI_DOUBLE, */
  /*                 scatter_root, MPI_COMM_WORLD); */
    
  /*   for ( int ix = 0; ix < len_array[mpi_rank] / width; ++ix ){ */
  /*     if ( pos_array[mpi_rank] != 0 && ix == 0 ) continue; // skip the 1st local row, unless it's the 1st global one. */
  /*     else if ( pos_array[mpi_rank] + ix + 1 != height && // skip the last local row, unless it's the last global one. */
  /*               ix == len_array[mpi_rank] - 1 ) continue; //  */
      
  /*     for ( int jx = 0; jx < width; ++jx ){ */
  /*       hij = temperature_sub[ ix * width +jx ]; */
  /*       hijW = ( jx-1 >= 0 ? temperature_sub[ ix * width + jx-1] : 0. ); */
  /*       hijE = ( jx+1 < width  ? temperature_sub[ ix * width + jx+1] : 0. ); */
  /*       hijS = ( (pos_array[mpi_rank]+ix+1) < height ? temperature_sub[ (ix+1) * width + jx] : 0.); */
  /*       hijN = ( (pos_array[mpi_rank]+ix-1) >= 0 ? temperature_sub[ (ix-1) * width + jx] : 0.); */
  /*       temperature_sub_new[ ix * width ] = hij + */
  /*         c * ( 0.25*( hijW + hijE + hijS + hijN ) - hij); */
  /*     } */
  /*   } */
  /*   if ( mpi_rank == 0 ) shift = 0; */
  /*   else shift = 1; */
  /*   MPI_Gatherv( temperature_sub_new + shift * width, sub_size * width, MPI_DOUBLE, */
  /*                grid_init, len_gather_array, pos_gather_array, MPI_DOUBLE, */
  /*                scatter_root, MPI_COMM_WORLD); */
  /* } */

  //compute average temperature
//  double avg = computeAverage(nextTimeEntries, width, height);

  /* double* avgDiffEntries = (double*) malloc(sizeof(double)*width*height); */
  /* for (int ix = 0; ix < width*height; ++ix) { */
  /*   avgDiffEntries[ix] = 0.; */
  /* } */

  /* /\* //compute differences (in nextTimeEntries since the contents are copied to grid_init) *\/ */
  /* if ( niter != 0) { */
  /*   memcpy(avgDiffEntries, nextTimeEntries, width*height*sizeof(double)); */
  /* } else { */
  /*   memcpy(avgDiffEntries, grid_init, width*height*sizeof(double)); */
  /* } */
  /* for ( int ix = 0; ix < width*height; ++ix ) { */
  /*   avgDiffEntries[ix] -= avg; */
  /*   avgDiffEntries[ix] = ( avgDiffEntries[ix] < 0 ? avgDiffEntries[ix]*-1.: avgDiffEntries[ix] ); */
  /* } */

  /* double avgDiff = computeAverage(avgDiffEntries, width, height); */
/* if (mpi_rank == 0) {// master process: */
/*   printf("average: %e\n", avg); */
/*   printf("average absolute difference: %e\n", avg_diff); */
/* } */

  // Clean up:
  /* free(len_gather_array); */
  /* free(pos_gather_array); */
  /* free(len_array); */
  /* free(pos_array); */
  /* free(temperature_sub); */
  /* free(temperature_sub_new); */
//  free(avgDiffEntries);


 if (mpi_rank == 0) free(grid_init);
 
 MPI_Finalize();


//  free(nextTimeEntries);
  
  return 0;
}

double computeNextTemp(const double* hij, const double* hijW, const double* hijE, const double* hijS, const double* hijN, const double* c)
{
  return (*hij) + (*c)*( ((*hijW) + (*hijE) + (*hijS) + (*hijN))/4 - (*hij));
}

double computeAverage(const double* tempArray, int width, int height)
{
  double sum=0.;
  for ( int ix = 0; ix < width*height; ++ix ) {
    sum += tempArray[ix];
  }
  return sum/(width*height);
}
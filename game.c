#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void read_and_alloc_data(const char *filename, int *rows, int *columns, int *laps, char **matrix)
{
	FILE *in_file = fopen(filename, "r");

	int items_read = fscanf(in_file, "%d %d %d", rows, columns, laps);
	if (items_read != 3) {
		printf("Eroare: Citire date din fisier\n");
	}

	*matrix = (char *)malloc((*rows) * 3 * (*columns) * sizeof(char));
	if (*matrix == NULL) {
		printf("Eroare: Alocare memorie pentru matrice\n");
		exit(1);
	}

	int index = 0;
	char value[3];

	for (int i = 0; i < *rows; i++) {
		for (int j = 0; j < *columns; j++) {
			int items_read = fscanf(in_file, "%s", value);
			if (items_read != 1) {
				printf("Eroare: Citire date din fisier\n");
				exit(1);
			}
			if (strlen(value) == 1) {
				(*matrix)[index++] = value[0];
				(*matrix)[index++] = '\0';
				(*matrix)[index++] = '\0';
			} else {
				(*matrix)[index++] = value[0];
				(*matrix)[index++] = value[1];
				(*matrix)[index++] = '\0';
			}
		}
	}

	fclose(in_file);
}

int main(int argc, char *argv[])
{
	int rank;
	int nProcesses;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);

	FILE *fin = fopen(argv[1], "r");
	int rows, columns, laps;
	int items_read = fscanf(fin, "%d %d %d", &rows, &columns, &laps);  // Citire header din file => mai rapid decat Bcast
	if (items_read != 3) {                                             // Prima linie - header
		printf("Eroare: Citire date din fisier\n");
	}
	fclose(fin);

	char *matrix;  // matricea o citesc doar in main process

	if (rank == 0) {  // Main process

		read_and_alloc_data(argv[1], &rows, &columns, &laps, &matrix);
	}

	int rows_per_process = rows / nProcesses;
	int remaining_rows = rows % nProcesses;

	int *displs = (int *)malloc(nProcesses * sizeof(int));
	int *counts = (int *)malloc(nProcesses * sizeof(int));

	for (int i = 0; i < nProcesses; i++) {
		counts[i] = rows_per_process * columns * 3;
		if (i < remaining_rows) {
			counts[i] += columns * 3;
		}
		displs[i] = i * rows_per_process * columns * 3;
		if (i < remaining_rows) {
			displs[i] += i * columns * 3;
		} else {
			displs[i] += remaining_rows * columns * 3;
		}
	}

	char *same_matrix = (char *)malloc(counts[rank] * sizeof(char));
	MPI_Scatterv(matrix, counts, displs, MPI_CHAR, same_matrix, counts[rank], MPI_CHAR, 0, MPI_COMM_WORLD);

	int *counts2 = (int *)malloc(nProcesses * sizeof(int));
	int *displs2 = (int *)malloc(nProcesses * sizeof(int));
	for (int i = 0; i < nProcesses; i++) {
		counts2[i] = rows_per_process * columns * 5;
		if (i < remaining_rows)
			counts2[i] += columns * 5;
		displs2[i] = i * rows_per_process * columns * 5;
		if (i < remaining_rows) {
			displs2[i] += i * columns * 5;
		} else {
			displs2[i] += remaining_rows * columns * 5;
		}
	}

	char *local_matrix = (char *)malloc(counts2[rank] * sizeof(char));
	memset(local_matrix, 'x', counts2[rank] * sizeof(char));

	char *work_matrix = (char *)malloc(counts2[rank] * sizeof(char));
	memset(work_matrix, 'x', counts2[rank] * sizeof(char));

	for (int i = 0; i < counts2[rank] / (columns * 5); i++) {
		for (int j = 0; j < columns; j++) {
			local_matrix[(i * columns + j) * 5] = same_matrix[(i * columns + j) * 3];
			if (same_matrix[(i * columns + j) * 3 + 1] != '\0')
				local_matrix[(i * columns + j) * 5 + 1] = same_matrix[(i * columns + j) * 3 + 1];
		}
	}

	char *vector_furnici_sus = (char *)malloc(columns * sizeof(char));
	memset(vector_furnici_sus, 's', columns * sizeof(char));

	char *vector_furnici_jos = (char *)malloc(columns * sizeof(char));
	memset(vector_furnici_jos, 'j', columns * sizeof(char));

	for (int z = 0; z < laps; z++) {
		int i = 0;
		int j = 0;
		for (i = 0; i < counts2[rank] / (columns * 5); i++) {
			for (j = 0; j < columns; j++) {
				work_matrix[(i * columns + j) * 5] = local_matrix[(i * columns + j) * 5];
				if (local_matrix[(i * columns + j) * 5 + 1] == '0' || local_matrix[(i * columns + j) * 5 + 1] == '1' || local_matrix[(i * columns + j) * 5 + 1] == '2' || local_matrix[(i * columns + j) * 5 + 1] == '3') {
					for (int k = 1; k < 5; k++) {
						char directie = local_matrix[(i * columns + j) * 5 + k];
						char color = local_matrix[(i * columns + j) * 5];

						switch (directie) {
						case '0':
							if (color == '0') {
								work_matrix[(i * columns + j) * 5] = '1';
								if (i == (counts2[rank] / (columns * 5)) - 1) {
									// printf("trebuie vector furnica trimis in jos\n");
									if (rank != nProcesses - 1) {
										vector_furnici_jos[j] = '3';
									} else {
										// printf("nu merge trimis in jos pentru ca este ultimul proces\n");
									}

								} else {
									for (int x = 1; x < 5; x++) {
										if (work_matrix[((i + 1) * columns + j) * 5 + x] == 'x') {
											work_matrix[((i + 1) * columns + j) * 5 + x] = '3';
											break;
										}
									}
								}
							}
							if (color == '1') {
								work_matrix[(i * columns + j) * 5] = '0';
								if (i == 0) {
									// printf("trebuie vector furnica trimis in sus\n");
									if (rank != 0) {
										vector_furnici_sus[j] = '1';
									} else {
										// printf("nu merge trimis in sus pentru ca este primul proces\n");
									}

								} else {
									for (int x = 1; x < 5; x++) {
										if (work_matrix[((i - 1) * columns + j) * 5 + x] == 'x') {
											work_matrix[((i - 1) * columns + j) * 5 + x] = '1';
											break;
										}
									}
									// work_matrix[((i - 1) * columns + j) * 5 + 1] = '1';
								}
							}
							break;
						case '1':
							if (color == '0') {
								work_matrix[(i * columns + j) * 5] = '1';
								if (j == 0) {
									// printf("Nu merge!\n");
								} else {
									for (int x = 1; x < 5; x++) {
										if (work_matrix[(i * columns + (j - 1)) * 5 + x] == 'x') {
											work_matrix[(i * columns + (j - 1)) * 5 + x] = '0';
											break;
										}
									}
									// work_matrix[(i * columns + (j - 1)) * 5 + 1] = '0';
								}
							}
							if (color == '1') {
								work_matrix[(i * columns + j) * 5] = '0';
								if (j == columns - 1) {
									// printf("Nu merge\n");
								} else {
									for (int x = 1; x < 5; x++) {
										if (work_matrix[(i * columns + (j + 1)) * 5 + x] == 'x') {
											work_matrix[(i * columns + (j + 1)) * 5 + x] = '2';
											break;
										}
									}
									// work_matrix[(i * columns + (j + 1)) * 5 + 1] = '2';
								}
							}
							break;
						case '2':
							if (color == '0') {
								work_matrix[(i * columns + j) * 5] = '1';
								if (i == 0) {
									// printf("trebuie vector furnica trimis in sus\n");
									if (rank != 0) {
										vector_furnici_sus[j] = '1';
									} else {
										// printf("nu merge trimis in sus pentru ca este primul proces\n");
									}
								} else {
									for (int x = 1; x < 5; x++) {
										if (work_matrix[((i - 1) * columns + j) * 5 + x] == 'x') {
											work_matrix[((i - 1) * columns + j) * 5 + x] = '1';
											break;
										}
									}
									// work_matrix[((i - 1) * columns + j) * 5 + 1] = '1';
								}
							}
							if (color == '1') {
								work_matrix[(i * columns + j) * 5] = '0';
								if (i == (counts2[rank] / (columns * 5)) - 1) {
									// printf("trebuie vector furnica trimis in jos\n");
									if (rank != nProcesses - 1) {
										vector_furnici_jos[j] = '3';
									} else {
										// printf("nu merge trimis in jos pentru ca este ultimul proces\n");
									}
								} else {
									for (int x = 1; x < 5; x++) {
										if (work_matrix[((i + 1) * columns + j) * 5 + x] == 'x') {
											work_matrix[((i + 1) * columns + j) * 5 + x] = '3';
											break;
										}
									}
									// work_matrix[((i + 1) * columns + j) * 5 + 1] = '3';
								}
							}
							break;
						case '3':
							if (color == '0') {
								work_matrix[(i * columns + j) * 5] = '1';
								if (j == columns - 1) {
									// printf("Nu merge\n");
								} else {
									for (int x = 1; x < 5; x++) {
										if (work_matrix[(i * columns + (j + 1)) * 5 + x] == 'x') {
											work_matrix[(i * columns + (j + 1)) * 5 + x] = '2';
											break;
										}
									}
									// work_matrix[(i * columns + (j + 1)) * 5 + 1] = '2';
								}
							}
							if (color == '1') {
								work_matrix[(i * columns + j) * 5] = '0';
								if (j == 0) {
									// printf("Nu merge!\n");
								} else {
									for (int x = 1; x < 5; x++) {
										if (work_matrix[(i * columns + (j - 1)) * 5 + x] == 'x') {
											work_matrix[(i * columns + (j - 1)) * 5 + x] = '0';
											break;
										}
									}
									// work_matrix[(i * columns + (j - 1)) * 5 + 1] = '0';
								}
							}
							break;
						default:
							break;
						}
					}
				}
			}
		}

		if (nProcesses != 1) {
			if (rank == 0) {
				MPI_Send(vector_furnici_jos, columns, MPI_CHAR, rank + 1, 0, MPI_COMM_WORLD);
			} else if (rank == nProcesses - 1) {
				MPI_Send(vector_furnici_sus, columns, MPI_CHAR, rank - 1, 0, MPI_COMM_WORLD);
			} else {
				MPI_Send(vector_furnici_jos, columns, MPI_CHAR, rank + 1, 0, MPI_COMM_WORLD);
				MPI_Send(vector_furnici_sus, columns, MPI_CHAR, rank - 1, 0, MPI_COMM_WORLD);
			}

			if (rank != 0) {
				MPI_Recv(vector_furnici_jos, columns, MPI_CHAR, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			}
			if (rank != nProcesses - 1) {
				MPI_Recv(vector_furnici_sus, columns, MPI_CHAR, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			}

			if (rank == 0) {
				for (int i = 0; i < counts2[rank] / (columns * 5); i++) {
					for (int j = 0; j < columns; j++) {
						if (i == ((counts2[rank] / (columns * 5)) - 1)) {
							for (int x = 1; x < 5; x++) {
								if (work_matrix[(i * columns + j) * 5 + x] == 'x' && vector_furnici_sus[j] != 's') {
									work_matrix[(i * columns + j) * 5 + x] = vector_furnici_sus[j];
									break;
								}
							}
						}
					}
				}
			} else if (rank == nProcesses - 1) {
				for (int i = 0; i < counts2[rank] / (columns * 5); i++) {
					for (int j = 0; j < columns; j++) {
						if (i == 0) {
							for (int x = 1; x < 5; x++) {
								if (work_matrix[(i * columns + j) * 5 + x] == 'x' && vector_furnici_jos[j] != 'j') {
									work_matrix[(i * columns + j) * 5 + x] = vector_furnici_jos[j];
									break;
								}
							}
						}
					}
				}
			} else {
				for (int i = 0; i < counts2[rank] / (columns * 5); i++) {
					for (int j = 0; j < columns; j++) {
						if (i == 0) {
							for (int x = 1; x < 5; x++) {
								if (work_matrix[(i * columns + j) * 5 + x] == 'x' && vector_furnici_jos[j] != 'j') {
									work_matrix[(i * columns + j) * 5 + x] = vector_furnici_jos[j];
									break;
								}
							}
						}
					}
				}

				for (int i = 0; i < counts2[rank] / (columns * 5); i++) {
					for (int j = 0; j < columns; j++) {
						if (i == ((counts2[rank] / (columns * 5)) - 1)) {
							for (int x = 1; x < 5; x++) {
								if (work_matrix[(i * columns + j) * 5 + x] == 'x' && vector_furnici_sus[j] != 's') {
									work_matrix[(i * columns + j) * 5 + x] = vector_furnici_sus[j];
									break;
								}
							}
						}
					}
				}
			}
		}
		for (int i = 0; i < columns; i++) {
			vector_furnici_jos[i] = 'j';
			vector_furnici_sus[i] = 's';
		}

		for (int i = 0; i < counts2[rank] / (columns * 5); i++) {
			for (int j = 0; j < columns; j++) {
				for (int x = 0; x < 5; x++) {
					local_matrix[(i * columns + j) * 5 + x] = work_matrix[(i * columns + j) * 5 + x];
					work_matrix[(i * columns + j) * 5 + x] = 'x';
				}
			}
		}
	}

	char *matrix_global = (char *)malloc(rows * columns * 5 * sizeof(char));
	MPI_Gatherv(local_matrix, counts2[rank], MPI_CHAR, matrix_global, counts2, displs2, MPI_CHAR, 0, MPI_COMM_WORLD);

	if (rank == 0) {
		FILE *fout = fopen(argv[2], "w");
		fprintf(fout, "%d %d\n", rows, columns);
		for (int i = 0; i < rows; i++) {
			for (int j = 0; j < columns; j++) {
				int index = (i * columns + j) * 5;
				for (int k = 0; k < 5 && matrix_global[index + k] != 'x'; k++) {
					fprintf(fout, "%c", matrix_global[index + k]);
				}
				fprintf(fout, " ");
			}
			fprintf(fout, "\n");
		}
		fclose(fout);
	}
	free(vector_furnici_jos);
	free(vector_furnici_sus);
	free(matrix_global);
	free(work_matrix);
	free(local_matrix);
	free(counts2);
	free(displs2);
	free(same_matrix);
	free(counts);
	free(displs);
	if (rank == 0)
	    free(matrix);
	    

	MPI_Finalize();
	return 0;
}

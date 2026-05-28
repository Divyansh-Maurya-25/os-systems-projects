/*
  Author: Divyansh Maurya (U11865935)
  Project 3: RAID Mapping Table Generator
  This program creates mapping tables for different RAID configurations
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main() {
    // Input variables: raid_level, num_disks, chunk_sz, total_sectors, filename
    char raid_level[4], filename[256];
    int num_disks, chunk_sz, total_sectors;

    if (scanf("%s %d %d %d %s", raid_level, &num_disks, &chunk_sz, &total_sectors, filename) != 5) {
        fprintf(stderr, "Error reading input.\n");
        return 1;
    }

    FILE *output = fopen(filename, "w");
    if (!output) {
        fprintf(stderr, "Error opening output file %s.\n", filename);
        return 1;
    }

    // Write table header with disk names
    fprintf(output, "\t");
    for (int disk = 0; disk < num_disks; disk++) {
        fprintf(output, "Disk%d", disk);
        if (disk < num_disks - 1) fprintf(output, "\t");
    }
    fprintf(output, "\n");

    // RAID 0: Simple striping across all disks
    if (strcmp(raid_level, "0") == 0) {
        // In RAID 0, data is striped across all disks with a given chunk size
        
        int stripe_width = num_disks * chunk_sz;
        
        for (int row = 0; row < total_sectors; row++) {
            fprintf(output, "Stripe%d", row);

            int stripe_group = row / chunk_sz;
            int offset_in_chunk = row % chunk_sz;       

            for (int disk = 0; disk < num_disks; disk++) {
                // Calculate logical sector number for this position
                int lsn = stripe_group * stripe_width + disk * chunk_sz + offset_in_chunk;
                fprintf(output, "\t%d", lsn);
            }
            fprintf(output, "\n");
        }
    }

    // RAID 0+1: Mirror then stripe
    else if (strcmp(raid_level, "01") == 0) {
        // RAID 01 stripes mirrored pairs. Each pair stores identical data.
        
        int active_disks = num_disks / 2;
        
        for (int row = 0; row < total_sectors; row++) {
            fprintf(output, "Stripe%d", row);

            for (int disk = 0; disk < num_disks; disk++) {
                // Find which disk in the mirrored set this belongs to
                int mirrored_idx = disk % active_disks;
                
                // Calculate the logical sector number
                int lsn = row * active_disks + mirrored_idx;
                fprintf(output, "\t%d", lsn);
            }
            fprintf(output, "\n");
        }
    }

    // RAID 1+0: Stripe then mirror
    else if (strcmp(raid_level, "10") == 0) {
        // RAID 10 creates mirrored pairs of striped data
        
        int mirror_pairs = num_disks / 2;
        
        for (int row = 0; row < total_sectors; row++) {
            fprintf(output, "Stripe%d", row);

            for (int disk = 0; disk < num_disks; disk++) {
                // Identify which pair this disk belongs to
                int pair_id = disk / 2;
                
                // Both disks in a pair hold the same data
                int lsn = row * mirror_pairs + pair_id;
                fprintf(output, "\t%d", lsn);
            }
            fprintf(output, "\n");
        }
    }

    // RAID 4: Single dedicated parity disk
    else if (strcmp(raid_level, "4") == 0) {
        // RAID 4 uses one disk for all parity information
        
        int data_only = num_disks - 1;
        int parity = num_disks - 1;

        for (int row = 0; row < total_sectors; row++) {
            fprintf(output, "Stripe%d", row);

            for (int disk = 0; disk < num_disks; disk++) {
                if (disk == parity) {
                    // Mark parity position
                    fprintf(output, "\tP");
                } else {
                    // Calculate data sector number
                    int lsn = row * data_only + disk;
                    fprintf(output, "\t%d", lsn);
                }
            }
            fprintf(output, "\n");
        }
    }

    // RAID 5: Distributed parity scheme
    else if (strcmp(raid_level, "5") == 0) {
        // RAID 5 spreads parity across all disks using left-asymmetric rotation
        
        int data_only = num_disks - 1;
        
        for (int row = 0; row < total_sectors; row++) {
            fprintf(output, "Stripe%d", row);

            // Calculate which disk holds parity for this stripe
            // Using left-asymmetric distribution pattern
            int parity_pos = (num_disks - 1) - (row % num_disks); 

            for (int disk = 0; disk < num_disks; disk++) {
                if (disk == parity_pos) {
                    fprintf(output, "\tP");
                } else {
                    // Find which data sector goes on this disk
                    // Based on rotation from parity position
                    int data_idx = (disk - parity_pos - 1 + num_disks) % num_disks;
                    
                    // Adjust for the fact that we have N-1 data sectors, not N
                    if (data_idx == num_disks - 1) data_idx = 0; 

                    int lsn = row * data_only + data_idx;
                    fprintf(output, "\t%d", lsn);
                }
            }
            fprintf(output, "\n");
        }
    }

    fclose(output);
    return 0;
}
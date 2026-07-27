#include <stdint.h>
#include <stdlib.h>

#include "h264_stream.h"

h264_stream_t* h264_new()
{
    h264_stream_t* h = (h264_stream_t*)calloc(1, sizeof(h264_stream_t));
    h->nal = (nal_t*)calloc(1, sizeof(nal_t));
    h->sps = (sps_t*)calloc(1, sizeof(sps_t));
    return h;
}

void h264_free(h264_stream_t* h)
{
    free(h->nal);
    free(h->sps);
    free(h);
}

int find_nal_unit(uint8_t* buf, int size, int* nal_start, int* nal_end)
{
    int i;
    *nal_start = 0;
    *nal_end = 0;

    i = 0;
    while (
        (i + 2 < size) &&
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) &&
        (i + 3 >= size || buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0 || buf[i+3] != 0x01)
        )
    {
        i++;
    }

    if (i + 2 >= size) { return 0; }

    if (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01)
    {
        i++;
    }

    if (i + 2 >= size || buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) { return 0; }
    i += 3;
    *nal_start = i;

    while (
        (i + 2 < size) &&
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0) &&
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01)
        )
    {
        i++;
    }

    if (i + 2 >= size) { *nal_end = size; return (*nal_end - *nal_start); }

    *nal_end = i;
    return (*nal_end - *nal_start);
}

int rbsp_to_nal(const uint8_t* rbsp_buf, const int* rbsp_size, uint8_t* nal_buf, int* nal_size)
{
    int i;
    int j     = 1;
    int count = 0;

    if (*nal_size > 0) { nal_buf[0] = 0x00; }

    for ( i = 0; i < *rbsp_size ; )
    {
        if ( j >= *nal_size )
        {
            return -1;
        }

        if ( ( count == 2 ) && !(rbsp_buf[i] & 0xFC) )
        {
            nal_buf[j] = 0x03;
            j++;
            count = 0;
            continue;
        }
        nal_buf[j] = rbsp_buf[i];
        if ( rbsp_buf[i] == 0x00 )
        {
            count++;
        }
        else
        {
            count = 0;
        }
        i++;
        j++;
    }

    *nal_size = j;
    return j;
}

int nal_to_rbsp(const uint8_t* nal_buf, int* nal_size, uint8_t* rbsp_buf, int* rbsp_size)
{
    int i;
    int j     = 0;
    int count = 0;

    for( i = 0; i < *nal_size; i++ )
    {
        if( ( count == 2 ) && ( nal_buf[i] < 0x03) )
        {
            return -1;
        }

        if( ( count == 2 ) && ( nal_buf[i] == 0x03) )
        {
            if((i < *nal_size - 1) && (nal_buf[i+1] > 0x03))
            {
                return -1;
            }

            if(i == *nal_size - 1)
            {
                break;
            }

            i++;
            count = 0;
        }

        if ( j >= *rbsp_size )
        {
            return -1;
        }

        rbsp_buf[j] = nal_buf[i];
        if(nal_buf[i] == 0x00)
        {
            count++;
        }
        else
        {
            count = 0;
        }
        j++;
    }

    *nal_size = i;
    *rbsp_size = j;
    return j;
}

//
//  TPCircularBuffer.h
//  Circular/Ring buffer implementation
//
//  https://github.com/michaeltyson/TPCircularBuffer
//
//  Created by Michael Tyson on 10/12/2011.
//
//
//  This implementation makes use of a virtual memory mapping technique that inserts a virtual copy
//  of the buffer memory directly after the buffer's end, negating the need for any buffer wrap-around
//  logic. Clients can simply use the returned memory address as if it were contiguous space.
//
//  The implementation is thread-safe in the case of a single producer and single consumer.
//
//  Virtual memory technique originally proposed by Philip Howard (http://vrb.slashusr.org/), and
//  adapted to Darwin by Kurt Revis (http://www.snoize.com,
//  http://www.snoize.com/Code/PlayBufferedSoundFile.tar.gz)
//
//
//  Copyright (C) 2012-2013 A Tasty Pixel
//
//  This software is provided 'as-is', without any express or implied
//  warranty.  In no event will the authors be held liable for any damages
//  arising from the use of this software.
//
//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software
//     in a product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and must not be
//     misrepresented as being the original software.
//
//  3. This notice may not be removed or altered from any source distribution.
//

#ifndef TPCircularBuffer_h
#define TPCircularBuffer_h

#include <stdbool.h>
#include <string.h>
#include <assert.h>

#ifdef __cplusplus
    extern "C++" {
        #include <atomic>
        typedef std::atomic_int atomicInt;
        #define atomicFetchAdd(a,b) std::atomic_fetch_add(a,b)
    }
#else
    #include <stdatomic.h>
    typedef atomic_int atomicInt;
    #define atomicFetchAdd(a,b) atomic_fetch_add(a,b)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void             *buffer;
    uint32_t           length;
    uint32_t           tail;
    uint32_t           head;
    volatile atomicInt fillCount;
    bool              atomic;
} TPCircularBuffer;

/*!
 * Initialise buffer
 *
 *  Note that the length is advisory only: Because of the way the
 *  memory mirroring technique works, the true buffer length will
 *  be multiples of the device page size (e.g. 4096 bytes)
 *
 *  If you intend to use the AudioBufferList utilities, you should
 *  always allocate a bit more space than you need for pure audio
 *  data, so there's room for the metadata. How much extra is required
 *  depends on how many AudioBufferList structures are used, which is
 *  a function of how many audio frames each buffer holds. A good rule
 *  of thumb is to add 15%, or at least another 2048 bytes or so.
 *
 * @param buffer Circular buffer
 * @param length Length of buffer
 */
#define TPCircularBufferInit(buffer, length) \
    _TPCircularBufferInit(buffer, length, sizeof(*buffer))
bool _TPCircularBufferInit(TPCircularBuffer *buffer, uint32_t length, size_t structSize);

/*!
 * Cleanup buffer
 *
 *  Releases buffer resources.
 */
void  TPCircularBufferCleanup(TPCircularBuffer *buffer);

/*!
 * Clear buffer
 *
 *  Resets buffer to original, empty state.
 *
 *  This is safe for use by consumer while producer is accessing
 *  buffer.
 */
void  TPCircularBufferClear(TPCircularBuffer *buffer);

/*!
 * Set the atomicity
 *
 *  If you set the atomiticy to false using this method, the buffer will
 *  not use atomic operations. This can be used to give the compiler a little
 *  more optimisation opportunities when the buffer is only used on one thread.
 *
 *  Important note: Only set this to false if you know what you're doing!
 *
 *  The default value is true (the buffer will use atomic operations)
 *
 * @param buffer Circular buffer
 * @param atomic Whether the buffer is atomic (default true)
 */
void  TPCircularBufferSetAtomic(TPCircularBuffer *buffer, bool atomic);

// Reading (consuming)

/*!
 * Access end of buffer
 *
 *  This gives you a pointer to the end of the buffer, ready
 *  for reading, and the number of available bytes to read.
 *
 * @param buffer Circular buffer
 * @param availableBytes On output, the number of bytes ready for reading
 * @return Pointer to the first bytes ready for reading, or NULL if buffer is empty
 */
static __inline__ __attribute__((always_inline)) void* TPCircularBufferTail(TPCircularBuffer *buffer, uint32_t* availableBytes) {
    *availableBytes = buffer->fillCount;
    if ( *availableBytes == 0 ) return NULL;
    return (void*)((char*)buffer->buffer + buffer->tail);
}

/*!
 * Consume bytes in buffer
 *
 *  This frees up the just-read bytes, ready for writing again.
 *
 * @param buffer Circular buffer
 * @param amount Number of bytes to consume
 */
static __inline__ __attribute__((always_inline)) void TPCircularBufferConsume(TPCircularBuffer *buffer, uint32_t amount) {
    buffer->tail = (buffer->tail + amount) % buffer->length;
    if ( buffer->atomic ) {
        atomicFetchAdd(&buffer->fillCount, -(int)amount);
    } else {
        buffer->fillCount -= amount;
    }
    assert(buffer->fillCount >= 0);
}

/*!
 * Access front of buffer
 *
 *  This gives you a pointer to the front of the buffer, ready
 *  for writing, and the number of available bytes to write.
 *
 * @param buffer Circular buffer
 * @param availableBytes On output, the number of bytes ready for writing
 * @return Pointer to the first bytes ready for writing, or NULL if buffer is full
 */
static __inline__ __attribute__((always_inline)) void* TPCircularBufferHead(TPCircularBuffer *buffer, uint32_t* availableBytes) {
    *availableBytes = (buffer->length - buffer->fillCount);
    if ( *availableBytes == 0 ) return NULL;
    return (void*)((char*)buffer->buffer + buffer->head);
}

// Writing (producing)

/*!
 * Produce bytes in buffer
 *
 *  This marks the given section of the buffer ready for reading.
 *
 * @param buffer Circular buffer
 * @param amount Number of bytes to produce
 */
static __inline__ __attribute__((always_inline)) void TPCircularBufferProduce(TPCircularBuffer *buffer, uint32_t amount) {
    buffer->head = (buffer->head + amount) % buffer->length;
    if ( buffer->atomic ) {
        atomicFetchAdd(&buffer->fillCount, (int)amount);
    } else {
        buffer->fillCount += amount;
    }
    assert((uint32_t)buffer->fillCount <= buffer->length);
}

/*!
 * Helper routine to copy bytes to buffer
 *
 *  This copies the given bytes to the buffer, and marks them ready for reading.
 *
 * @param buffer Circular buffer
 * @param src Source buffer
 * @param len Number of bytes in source buffer
 * @return true if bytes copied, false if there was insufficient space
 */
static __inline__ __attribute__((always_inline)) bool TPCircularBufferProduceBytes(TPCircularBuffer *buffer, const void* src, uint32_t len) {
    uint32_t space;
    void *ptr = TPCircularBufferHead(buffer, &space);
    if ( space < len ) return false;
    memcpy(ptr, src, len);
    TPCircularBufferProduce(buffer, len);
    return true;
}

/*!
 * Deprecated method
 */
static __inline__ __attribute__((always_inline)) __deprecated_msg("use TPCircularBufferSetAtomic(false) and TPCircularBufferConsume instead")
void TPCircularBufferConsumeNoBarrier(TPCircularBuffer *buffer, uint32_t amount) {
    buffer->tail = (buffer->tail + amount) % buffer->length;
    buffer->fillCount -= amount;
    assert(buffer->fillCount >= 0);
}

/*!
 * Deprecated method
 */
static __inline__ __attribute__((always_inline)) __deprecated_msg("use TPCircularBufferSetAtomic(false) and TPCircularBufferProduce instead")
void TPCircularBufferProduceNoBarrier(TPCircularBuffer *buffer, uint32_t amount) {
    buffer->head = (buffer->head + amount) % buffer->length;
    buffer->fillCount += amount;
    assert((uint32_t)buffer->fillCount <= buffer->length);
}

#ifdef __cplusplus
}
#endif

#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
TaskList.ino
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// This is the worst possible programming method that is extremely likely
// to fail in the future when the data structures or call interface changes!
// The fix is to use an include file for the TaskSnapshot_t and
// uxTaskGetSnapshotAll declarations.
//----------------------------------------

// These definitions came from the IDF in
// components/freertos/esp_additions/include/freertos/freertos_debug.h

/**
 * @brief Task Snapshot structure
 *
 * - Used with the uxTaskGetSnapshotAll() function to save memory snapshot of each task in the system.
 * - We need this structure because TCB_t is defined (hidden) in tasks.c.
 */
typedef struct xTASK_SNAPSHOT
{
    void * pxTCB;               /*!< Address of the task control block. */
    StackType_t * pxTopOfStack; /*!< Points to the location of the last item placed on the tasks stack. */
    StackType_t * pxEndOfStack; /*!< Points to the end of the stack. pxTopOfStack < pxEndOfStack, stack grows hi2lo
                                 *  pxTopOfStack > pxEndOfStack, stack grows lo2hi*/
} TaskSnapshot_t;

/**
 * @brief Fill an array of TaskSnapshot_t structures for every task in the system
 *
 * - This function is used by the panic handler to get a snapshot of all tasks in the system
 *
 * @note This function should only be called when FreeRTOS is no longer running (e.g., during a panic) as this function
 *        does not acquire any locks. For safe usage with scheduler running, use vTaskSuspendAll() before calling
 *        this function and xTaskResumeAll() after.
 *
 * @param[out] pxTaskSnapshotArray Array of TaskSnapshot_t structures filled by this function
 * @param[in] uxArrayLength Length of the provided array
 * @param[out] pxTCBSize Size of the a task's TCB structure (can be set to NULL)
 * @return UBaseType_t Number of task snapshots filled in the array
 */

extern "C"
{
UBaseType_t uxTaskGetSnapshotAll( TaskSnapshot_t * const pxTaskSnapshotArray,
                                  const UBaseType_t uxArrayLength,
                                  UBaseType_t * const pxTCBSize );
}

//----------------------------------------
// Display the task list
//----------------------------------------
void rtkTaskList(Print * display)
{
    const size_t bufferLength = 65536;
    UBaseType_t entries;
    int i;
    int index;
    int j;
    uint16_t * snapshotIndex;
    TaskSnapshot_t * snapshotBuffer;
    uintptr_t stackUse;
    uintptr_t stackEnd;
    uintptr_t stackTop;
    TaskHandle_t taskHandle;
    const char * taskName;
    UBaseType_t tcbBytes;
    uint16_t temp;
    uintptr_t totalStackUse;

    do
    {
        snapshotIndex = nullptr;

        // Allocate the snapshot buffer
        snapshotBuffer = (TaskSnapshot_t *)rtkMalloc(bufferLength, "Task snapshot buffer");
        if (snapshotBuffer == nullptr)
            break;

        // Suspend the tasks to take the snapshot
        // NO tasks are allowed to run during the snapshot
        // Take the snapshot
        vTaskSuspendAll();
        entries = uxTaskGetSnapshotAll(snapshotBuffer, bufferLength, &tcbBytes);
        xTaskResumeAll();

        // Allocate the index buffer for sorting
        snapshotIndex = (uint16_t *)rtkMalloc(entries * sizeof(*snapshotIndex), "Task snapshot index buffer");
        if (snapshotIndex == nullptr)
            break;

        // Initialize the sort index
        for (index = 0; index < entries; index++)
            snapshotIndex[index] = index;

        // Bubble sort the tasks by name
        for (i = 0; i < entries - 1; i++)
        {
            for (j = i + 1; j < entries; j++)
            {
                if (strcasecmp(pcTaskGetName((TaskHandle_t)snapshotBuffer[snapshotIndex[i]].pxTCB),
                               pcTaskGetName((TaskHandle_t)snapshotBuffer[snapshotIndex[j]].pxTCB)) > 0)
                {
                    // Switch the two entries
                    temp = snapshotIndex[i];
                    snapshotIndex[i] = snapshotIndex[j];
                    snapshotIndex[j] = temp;
                }
            }
        }

        // Display the number of tasks
        display->printf("%d Tasks\r\n", entries);

        // Walk the list of task snapshots
        totalStackUse = 0;
        for (index = 0; index < entries; index++)
        {
            taskHandle = (TaskHandle_t)snapshotBuffer[snapshotIndex[index]].pxTCB;
            stackTop = (uintptr_t)snapshotBuffer[snapshotIndex[index]].pxTopOfStack;
            stackEnd = (uintptr_t)snapshotBuffer[snapshotIndex[index]].pxEndOfStack;
            stackUse = stackEnd - stackTop;
            totalStackUse += stackUse;
            taskName = pcTaskGetName(taskHandle);
            display->printf("%p %p: %6d Stack bytes%s%s\r\n",
                            stackEnd,
                            stackTop,
                            stackUse,
                            taskName ? ", " : "",
                            taskName ? taskName : "");
        }
        if (totalStackUse)
            display->printf("%d total stack bytes in use\r\n", totalStackUse);
    } while (0);

    // Release the buffers
    if (snapshotIndex)
        rtkFree(snapshotIndex, "Task snapshot index buffer");
    if (snapshotBuffer)
        rtkFree(snapshotBuffer, "Task snapshot buffer");
}

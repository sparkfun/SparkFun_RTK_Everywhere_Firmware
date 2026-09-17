enum Im19UpdateResult
{
    IM19_UPDATE_FAILED = 0,
    IM19_UPDATE_SUCCESS,
    IM19_UPDATE_RETRY, // lost frames (or no response) - caller should re-stream the source and call again
};

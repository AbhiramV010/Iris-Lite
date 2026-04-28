bool SharedFrameBuffer::getFrame(uint64_t index, std::vector<uint8_t>& out)
{
    if (!isValid())
        return false;

    uint64_t headBefore = getHead();

    // -------- STALE PROTECTION (UNCHANGED) --------
    if (headBefore > bufferSize && index + bufferSize < headBefore)
    {
        logWarn("Frame dropped (stale): " + std::to_string(index));
        return false;
    }

    size_t slot = index % bufferSize;

    // -------- READ ATTEMPT 1 --------
    uint32_t size1 = frame_sizes[slot];

    if (size1 == 0 || size1 > slotSize)
        return false;

    out.resize(size1);

    std::memcpy(out.data(),
        frame_buffer + slot * slotSize,
        size1);

    // -------- LIGHT VALIDATION (NON-FATAL) --------
    uint64_t headAfter = getHead();
    uint32_t size2 = frame_sizes[slot];

    // IMPORTANT CHANGE:
    // only treat as failure if BOTH changed (not head alone)
    bool overwritten = (size1 != size2);

    if (overwritten)
    {
        // -------- RETRY ONCE (IMPORTANT FIX) --------
        std::this_thread::sleep_for(std::chrono::microseconds(50));

        uint32_t sizeRetry = frame_sizes[slot];

        if (sizeRetry == 0 || sizeRetry > slotSize)
            return false;

        if (sizeRetry != size1)
        {
            logWarn("Frame overwrite confirmed: " + std::to_string(index));
            return false;
        }

        out.resize(sizeRetry);

        std::memcpy(out.data(),
            frame_buffer + slot * slotSize,
            sizeRetry);
    }

    return true;
}
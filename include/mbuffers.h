/*
    Monitor buffers definitions.
    Author: Mateusz Gurdała
    Part of 2023 B.Sc Thesis

    To add a new buffer, add a new line:
    MBUFF(BUFFER_NAME, BUFFER_TYPE, BUFFER_SIZE)

    Buffer Types:
        DT_ONDEMAND - data is buffered at kernel/server level and extracted on user request
        DT_REALTIME - data is sent directly to the host

    Buffer size must be at least 1.
    When DT_REALTIME buffer is declared size is automatically set to 0.
*/
#define MBUFFERS() \
	MBUFF(msg, DT_ONDEMAND, 2) \ 
    MBUFF(rtmsg, DT_REALTIME, 0) \
    MBUFF(schinfo, DT_REALTIME, 0)

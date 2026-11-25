// time.js
//
// richc's monotonic clock on a web page (richc/time.h): performance.now(), which counts
// milliseconds from the page's load and never goes backwards. Browsers coarsen it, to guard
// against timing attacks, to tens of microseconds or more.

// The clock. It needs nothing from the instance, but has a bind like the others.
export function createTime() {
    return {
        imports: {
            time_now_ms: () => performance.now(),
        },

        bind() {
        },
    };
}

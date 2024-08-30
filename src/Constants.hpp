
#define DEBUG_NONE          0x0000  // No debug output (release build)
#define DEBUG_INFO          0x0001  // General output, like startup variables and status
#define DEBUG_SERIAL        0x0002  // Serial commands and replies
#define DEBUG_MOUNT         0x0008  // Mount processing output
#define DEBUG_CTRL          0x0020  // Other misc. output
#define DEBUG_JOBS          0x0040  // Other misc. output

#define DEBUG_ANY           0xFFFF  // All debug output

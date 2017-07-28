#ifndef _SVM_H_
#define _SVM_H_

#ifndef SVM_NAME
#define SVM_NAME "dev_mem"
#endif

#ifndef SVM_SIZE
#define SVM_SIZE ((1<<20)*4)
#endif

#ifndef SVM_MAGIC
#define SVM_MAGIC "#*"
#endif

#ifndef SVM_OFFSET
#define SVM_OFFSET 0x10
#endif

#ifndef SVM_MINOR
#define SVM_MINOR 233
#endif

#ifndef SVM_CAPACITY
#define SVM_CAPACITY 0x100
#endif

#endif /* _SVM_H_ */

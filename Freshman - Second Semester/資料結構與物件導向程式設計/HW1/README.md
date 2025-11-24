113550153's To-Do list Program Instruction
---
This To-Do list program allows multiple tasks with identical attribute, and the default task order follows the input sequence. Thus, you can even create the same task multiple times as well as modify designated task data by entering its index in the list. Besides, each single task command is in O(logN) (Though maybe with big coefficient x_x).

This To-Do list also has basic syntax and input value checking system, preventing careless typo or extra space input which may result in disastrous problem. In worst case, you can still undo or redo your previous operation to fix the wrong operation.

#### Note: This program requires GNU GCC support, please ensure this code is compiled by new GNU GCC version.

### The class and attributes corresponding table

Here's the table for each class and its corresponding attributes table.
This table show each class for task data.

| Task name | Category | Importance | Completed |
| :--: | :--: | :--: | :--: |
| task_name  | category_name | Yes/No | Yes/No |

### Add task
- You can ignore a item by fill a '-' on that item. (But Task Name is mandatory)
*(You might be able to add data without entering the last completed attribute, and it will follow the value of the importance. But it is not the official way to add item, I suggest using '-' to use default value)*
- Format: ```add [Task name] [Category] [Importance] [Completed] [Note(optional)]```
    - E.G. ```add Programming Learning yes no```
    - E.G. ```add Running Exercise - no It's on Monday.```
### View task
- All tasks, Format: ```view all [category/importance/completed (optional)] ``` (sort_base)
    - E.G. ```view all importance```
- Name-Based, Format: ```view task [Task name]```
    - E.G. ```view task Programming```
- Category-Based, Format: ```view category [Category]```
    - E.G. ```view category Learning```
- Importance-Based, Format: ```view importance [Importance]```
    - E.G. ```view importance yes```
- Completed-Based, Format: ```view completed [Completed]```
    - E.G. ```view completed no```
- Order-Based, Format: ```view order [order]```
    - E.G. ```view order 3```
### Delete task
- Delete all tasks with specific feature, Format: ```delete all [task/category/importance/completed] [attribute]```
    - E.G. ```delete all category Exercise```
    - E.G. ```delete all importance no```
    - E.G. ```delete all task Programming```
- Delete specific item, Format: ```delete [task/category/importance/completed] [attribute] [Order in the list]```
    - E.G. if you want to delete the first "Programming" task, use ```delete task Programming 1```
    - E.G. if you want to delete the first task in Exercise category, use ```delete category Exercise 1```
    - E.G. if you want to delete the second uncompleted task, use ```delete completed n 2```
### Modify task
- You can ignore a item by fill a '-' on that item. (And it will remain its original value)
- Modify all tasks with specific feature, Format: ```modify all [task/category/importance/completed] [attribute] [new_task_name] [new_category] [new_importance] [new_complete_state]```
    - E.G. ```modify all category Exercise - - yes no``` , in this case, the importance and complete state of all the tasks in Exercise, will be set by yes and no, respectively.
- Modify specific item, Format: ```modify [task/category/importance/completed] [attribute] [Order in the list] [new_task_name] [new_category] [new_importance] [new_complete_state]```
    - E.G. if you want to modify the first "Programming" task, and you use ```modify task Programming 1 - Homework - -```, the first "Programming" task will be moved to category Homework
### Undo or redo the operation
- By the command ```undo``` and ```redo``` , you can go your previous or next data state easily. *(You can't use it for cancelling system setting operation, such as reset default value.)*
### Setting default value
- As you add data, you can use '-' on the specific item to use default value, and this To-Do list allows you reset the default value.
- You **can't** use '-' to ignore item in this case.
- Format: ```set_default [Default_category] [Default_importance] [Default_complete_state]```
### Export list data
- To export current list to a csv file, use ```export```
- The csv file will be saved in the current folder.
- **Note: If there is already a file exists, this operation will overwrite the old file.**
### Show the To-Do list statistics
- To show the current statistics info, use ```show_statistics```
### Show the data operation log
- To show the data operation log, use ```show_log```
- Any set operation are actually combined by multiple single operation, and you might notice that.
### Exit the program
- To exit, use ```exit```

The full instruction end here. For other functionality, or you find a glitch of it, ask this program, use **```help```** or greet this program.
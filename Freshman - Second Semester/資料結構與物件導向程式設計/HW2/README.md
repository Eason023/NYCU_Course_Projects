113550153's Library Management System Instruction
---
This program provides a **visualized Library Record System with AI-driven features**. You can register books, check out or return the book, view the borrow record, search book by various criteria, import and export system data from .csv file, check several kinds of book statistics just like a library administrator!

On top of that, **this library record system is based on an efficient database and search algorithm**, so that each operation will be completed in rather short time, even when the database includes huge amount of books. By the well-designed database structure, you are allowed to add books with identical attributes. (For example, there might be the same books from different publisher, this system will process these book data correctly.)

Last but not least, **this LRS system works with a pure C/C++ version of LLM (Large Language Model)!** With the **local Llama-3.1-8B-Instruct-Q8 LLM model**, you can even **use an agent-like AI without the Internet**. It can **not only write a book summary but also to analyze the book information in the system and even much more!**

#### Note: You can adjust the subwindow layout. However, don't combine or minimize the subwindow (e.g. Booklist window) in the program (Although it behaves like a normal program window), or the program will be terminated automatically!

### Minimum specifications: (For LLM feature) (GPU mode)
> - A modern CPU.
> - 16GB of RAM. (If VRAM is insufficient.)
> - A Nvidia GPU with cuda compute capability 7.5.(At least GTX16 or RTX20) At least 10GB of VRAM. (Insufficient Vram will cause significant performance degradation)
> - Expectation: ~10 seconds of generation time.
### Recommended specifications: (For LLM feature) (GPU mode)
> - A modern CPU.
> - 16GB of RAM.
> - RTX2080ti 11GB or higher (At least 10GB of VRAM, more than 12GB would be better, compute capability 7.5).
> - Expectation: within 2~5 seconds of generation time.

This program will automatically change the CPU setting based on your PC device.

### Environment
> - Windows 10/11
> - Visual Studio 2022 msvc (with CUDA integration)
> - C++20
> - OpenGL3
> - OpenMP
> - CUDA v12.8 or newer

#### The LLM response should within 2~7 seconds if you are in the correct environment.

#### p.s. I also provide a pure cpu version for this program. Because of the low performance (1~2 minutes generation time in typical case), this instruction is for GPU only. You can still ask me to provide a pure CPU version instruction if you want.

---
> ## How to execute this program
> **This program must be executed using the Windows platform (VS 2022 msvc). To run this program, please compile the program in "x64 native tools command prompt for vs 2022" using the following command in the `Source code` directory:**
```
cl /EHsc /std:c++20 /MT ^
    HW2_113550153_林郁軒.cpp ^
    imgui\imgui.cpp imgui\imgui_stdlib.cpp imgui\imgui_draw.cpp imgui\imgui_widgets.cpp imgui\imgui_demo.cpp imgui\imgui_tables.cpp ^
    backends\imgui_impl_glfw.cpp backends\imgui_impl_opengl3.cpp ^
    /Iexternal\llama.cpp\include ^
    /Iexternal\llama.cpp\ggml\include ^
    /Iglfw\include ^
    /Iimgui ^
    /Ibackends ^
    /openmp ^
/link ^
    /LIBPATH:"glfw\lib-vc2022" glfw3_mt.lib ^
    opengl32.lib gdi32.lib winmm.lib ws2_32.lib user32.lib shell32.lib uuid.lib ^
    external\llama.cpp\build\common\Release\common.lib ^
    external\llama.cpp\build\src\Release\llama.lib ^
    external\llama.cpp\build\ggml\src\Release\ggml.lib ^
    external\llama.cpp\build\ggml\src\Release\ggml-base.lib ^
    external\llama.cpp\build\ggml\src\ggml-cuda\Release\ggml-cuda.lib ^
    external\llama.cpp\build\ggml\src\Release\ggml-cpu.lib ^
    /OUT:HW2_113550153_林郁軒_GUI.exe
```
> If there is any warning related to unicode, just ignore the warning.
> **After the compilation is completed**, the **HW2_113550153_林郁軒_GUI.exe** executable file will be generated. Double-click the file to execute it.
> **P.S. Please make sure that code is compiled by msvc cl.exe in "x64 native tools command prompt for vs 2022", and the CUDA version is v12.8 or newer, otherwise an error may be reported.**
> #### Note: This program requires Windows VS 2022 msvc kits, C++20, OpenMP, CUDA, and OpenGL3 support, please ensure you are in the  correct environment.
---
## The book attributes table
Here's the table of the book attributes.
This table shows every attributes for each book data.

| Title | Genre | Author | Publisher | Year | Overview | Total_stock | Available_copies | Rating | Borrow_times (Views) |
| :--: | :--: | :--: | :--: | :--: | :--: | :--: | :--: | :--: | :--: |
| book_name  | book_genre | author_name (1~3 people) | publisher | publication_year (1800~2100) | summary | total_book_stock (1~100) | stock_remaining | reader_rating (1.0~10.0) | number_of_views |
## Register new book
> ### New book register block
> 
> *A window on upper left of the GUI for registering new book.*
> 
> This part allows you to add new book into this system by entering new book information, including book title, book genre, book author(up to 3), publisher, book publication year, the total stock that are going to be added to this system, and even the pre-rating. Besides, you can also add an overview of the book.
> 
> Lastly, click the _Add_ button to store this book in the system. If there is lack of the book registration information or other error, a warning notification will pop up on lower left of the GUI.
> 
> p.s. Please make sure you just choose the correct number of authors before you add the book.
> **Note: There is an 256 characters limitation for the main attributes except overview.**
## View and search books
> ### Book list block
> 
> *A window on right part of the GUI for searching, viewing, and others.*
> 
> This part include a search bar, a display number slider, a search option combo, a partial match toggle, and the book list display page.
> 
> This book list will show every book in lexicographical ascending order if you left the search bar blank. By entering the search text, you can search a book by title, genre, author, publisher, and publication year. For the publication year search, it will be sorted by title in lexicographical ascending order. Otherwise, it will be sorted in lexicographical ascending order based on the type of search you chose.
> 
> In addition, this system allow search with partial match pattern, which means you don't need to enter full name to search the book data. This feature support for 4 kinds of search except search by year. However, this may increase the system load if huge amount of book data exist (e.g. more than millions of books).
> 
> In the Book list block, there is a book list display page. You can view detailed book information by click the title of the book you want to view (e.g. number of views, overview). (You have to click the title text)
> 
> **Note: The list can display up to 50 book information each page, you can check other book by change the display page.**
## Check out or return a book (or rate the book)
Refer to book list display page instruction, you will see the detailed book page of the book as you click a book title. Meanwhile, there are Borrow button and Return button on the end of the book information page. After filling the username and selecting the number of books, you can check out a book by click *borrow* button. Just as borrowing a book, select the borrower username and click the *return* button to return a book. (Scroll down the page if you don't see the button)

You can also rate this book by setting the rating slider and clicking the _Rate this book_ button. (Each new rating counts for 15% of the overall rating)

A notification on lower left of the GUI will pop up right after you check out a book, return a book, or rate a book.

**Note: If there is no available copies or all of the book have been returned, the _Borrow_ or _Return_ button will be replace by the notification text.**
## View the most popular books and the top rated books
> ### Advanced operation block
> 
> *A window on lower left of the GUI for database operation.*
> 
> This part show the library book database information, including various kinds of statistics, borrowing record, etc. In addition, you can do work with the AI agent or export the database file to transfer the data to other system.
> 
> Switch the combo box to view different system information. For example, you can view the top 5 popular books and the top 5 rated books by switch the combo box to "The most popular books" or "The top rated books".
> 
> p.s. You might observe that "The most popular books" and table are empty initially, because there is no borrow record in the beginning. (Unless you import the data from the database file)
> **Note: The system will not automatically identify the same book. If you add a book that already exists in the system, there will be multiple copies.**
## LLM model and Agent-like AI
In the combo box of advanced operation block, there is an option "LLM Agent (Llama-3.1-8B-Instruct-Q8_0)", switch the combo box to that option. Click send to send your request. **The LLM model might take several seconds to generate response.**

**3 kinds of LLM mode based on different function:**
- ### Book Expert mode (For any book question)
    - **Book Expert:** You can ask any book information even if it is not in the current Library system.
    - **Book Summarizer:** You can ask it to provide a summary (such as book summary) by the information you provide.
    - **Book Recommendation:** You can get a highly personalized book recommendation based on your preference.
    - **Examples:**
        - What's the book "The wonderful story of henry sugar and six more" about?
        - Make a summary based on this "some content\.\.\."
        - I'd like to read exotic books, can you recommend some books?
- ### Library Agent mode (For booklist or database question)
    - **Library Expert:** You can ask any question **related to the library database**. It can **analyze the system information for you**.
    - **Booklist Analyzer:** You can get a highly personalized personalized book recommendation, or other information you desired **based on the books on the current displayed booklist page.**
    - **Booklist Summarizer:** You can get comprehensive system information with this AI Agent.
    - **Examples:**
        - What's the best book? Explain your choice.
        - What's the book's content with "GUI" in title about?
        - What can you conclude based on data in this library system?
        - Can you notice some characteristics of these books?
        - I am going to study for a degree. Can you recommend a book?
    - **The AI agent do not have any specific user's borrowing information for security consideration.**
- ### General mode (For system question or other)
    - **System Helper:** If you encounter **any confusion related to this program**, you can **ask it to give you the instruction.**
    - **General LLM Assistant:** Just like a normal LLM model, you can ask it anything. It can work without the Internet!
    - **Examples:**
        - How to view database information?
        - What can I do in this system?
        - Have you heard of NYCU in Taiwan?

**You have to choose the correct mode for your needs, or model might output incorrect information or refuse to answer.**

**Each response is independent, model has no memory. (Because model might be used by different people, I discard the model's memory intentionally.)**

**Note: The model may occasionally output meaningless data due to its limitations. Please ignore it, you can just re-send or modify the request. Any information provided by the LLM model might be inaccurate, please check the validity of the data.**
## View the library book statistics
In the combo box of advanced operation block, there is an option "Library book statistics". You can view the database statistics by switch combo box to that option.

The library book database include following statistics:
- Number of various books: How many kinds of different books.
- Number of various genres: How many kinds of different genres.
- Number of various authors: How many different authors.
- Number of various publishers: How many different publishers.
- Total book inventory: The total number of books stored in the system.
- Number of available books: The number of available books for borrowing.
- Borrowing ratio: The ratio of borrowed book and total books stock.
- Total borrowing times: The total number of borrowing times.
- The average book rating: The average rating of different books.
## View the borrowing record
In the combo box of advanced operation block, there is an option "Borrower record", switch the combo box to that option.

You can view the borrowing record sorted by the borrowing date and the borrower's username. Just like viewing books, you can view other records by clicking *previous page* or *next page* buttons.

Select the filter to mark overdue borrowing record. (Borrowing date will be marked as red)

By clicking the book title, the book detail will appear with the borrower (for returning) being selected. You can return the borrower's book in a straight forward way.
## Import or export library data (Book data and Borrowing data)
In the combo box of advanced operation block, there is an option "Database management", switch the combo box to that option.

To import the library data from a LRS format .csv file, click the button "Import from current folder". The file should be named as _LRS_DB.csv_.
**Do not import a file made by yourself, it might not follow the LRS format and result in some terrible issues.**

**_Warn: If the imported file didn't follow the LRS format, it might break the current operating database file structure! Please check the file is intact before you import the file!_**

You can export all of the library data to a LRS format .csv file by click the button "Export to current folder". The exported file will be named as _LRS_DB.csv_. In addition, there is a file length predictor, so you can estimate the output file size before you export it.

**Note: If there is already a file exists, the export operation might overwrite the old file and therefore break the file. Make sure there is no file named _LRS_DB.csv_ before exporting.**
## Exit the program
To exit, click "Exit System" button in "Database management" of advanced operation block.

**Note: Once you leave the system, all data in this system will be cleared. Make sure you have back up the data before you leave.**
## Data Structure Speed Test (compare to STL map)
**You can use the code "DatastructureTest.cpp" to test the treap structure in my program, you will see its advantage as the find_by_order_num increasing!**
## Book list example
**If there is a folder named _LRS Example_ in the `Source code` directory, it might include the example _LRS_DB.csv_ file. You can import it to test this program.**

---

The full instruction end here. For other functionality, or you find a glitch of it, greet this program. Ask the program to debug itself. The AI assistant would like to help you deal with it.
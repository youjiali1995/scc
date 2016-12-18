# scc

自学《编译器设计》后写的一个简单的C编译器s(simple)cc。

-----------------

## 前端
词法分析得到词素流后进行基础的递归下降语法分析得到AST。

## 后端
直接由AST生成对应的汇编代码，采用`1-TOSCA`分配寄存器。

## 完成度
1. 数据类型：
    * int(只支持10进制)
    * char
    * float
    * double
    * 多重指针
    * 一维数组

2. 支持所有运算符和相关操作。

3. 支持：
    * if-else 
    * do-while
    * while
    * for 
    * 不支持switch-case

4. 支持嵌套作用域、重复声明和类型检查。

5. 支持函数，不支持变参函数，手动将`printf`插入符号表来调用。

## 用法
```bash
$ make
$ ./scc test/heart.c
$ ./scc test/nqueen.c
```

## 例子
1. ![test/heart.c](https://github.com/zlwgx/scc/blob/master/test/heart.c)

![image](https://github.com/zlwgx/scc/blob/master/doc/heart.png)

2. ![test/nqueen.c](https://github.com/zlwgx/scc/blob/master/test/nqueen.c)

![image](https://github.com/zlwgx/scc/blob/master/doc/nqueen.png)

## TODO

1. 复杂声明
2. 结构体、联合、多重数组
3. 预处理


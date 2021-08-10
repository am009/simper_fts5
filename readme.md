# simper sqlite3 fts5 tokenizer

- splits CJK character into single token.
- Based on Unicode61 Tokenizer.
- built as sqlite loadable extensions (shared library)

- 基于Unicode61 Tokenizer，将CJK字符作为单独的Token返回
- 插件式加载

### Unicode61
xToken这个回调函数，需要传入指向Token缓冲区的指针，缓冲区的大小，以及对应原输入的起始和结束位置。下面这个算法扫描的是原输入，Token缓冲区aFold

```
while () 大循环
  将输出指针重新指向Token缓冲区的开头
  while() 扫描分隔符
    如果当前扫描位置超过了字符串末尾，直接结束-跳转到tokenize_done
    如果是Unicode
      读入码点，判断是不是分隔符，是则跳跃到non_ascii_tokenchar
    否则-不是unicode
      该字符判断是不是分隔符，是则跳跃到ascii_tokenchar 不是则继续下一轮循环

  While(扫描位置小于字符串末尾)
    如果当前剩余buf空间小于6字节
      扩大一倍空间
    如果接下来是Unicode
      读入码点，如果是Token字符
non_ascii_tokenchar:
            移除unicode字符的顶部标音，并转小写？
            写入Token缓冲区
          否则- 不是Token字符
            Break出循环
    否则-不是Unicode
      如果是分隔符
        break退出循环
      如果是Token字符
        转换为小写，写入缓冲区
  调用Token回调处理缓冲区的Token – 之后回到开头，把指针重新设置到开头
tokenize_done:
  返回成功

```

修改的话，只需要修改Unicode且是Token字符的部分，写入一个Token的时候也跳出循环即可，需要注意更新ie-对应输入字符串中的ind之后再跳出循环。或者在代码中编码常用汉字信息，只对常用汉字这么做。

这样修改的话，是基于Token字符判断之后的，意味着标点符号什么的都已经被过滤掉了。


### Unicode 种类判断

aFts5UnicodeMap保存了不同种类的Unicode字符集的开始字符的低16位，目标是通过二分搜索找到所处的字符集。得到iRes下标就是输入字符所处的字符集。

Unicode码点右移16位，在aFts5UnicodeBlock中的下标就是需要在aFts5UnicodeMap里开始搜索的下标。现在确实Unicode只到了2FA1F，但是它额外放了一些Tags在E0000 — E007F，所以这个部分还是很必要的。

这个二分搜索也是Lower bound。如果字符不是起始字符，则该字符处于iRes和iLo中间的字符集内。如果是的话（恰好等于aFts5UnicodeMap中的某个值），iLo会指向该字符，而iRes会指向前一个字符集。

aFts5UnicodeData的低五位是Flag，大部分情况下就返回对应Data的低5位作为类别。

类别数据来自：http://www.unicode.org/reports/tr44/#GC_Values_Table 可能，总之两个字母代表一个类别，由sqlite3Fts5UnicodeCatParse去parse。所以根据设置的下标值我们也可以看到对应的类别。Category数组设置为1就表示是Token Char，否则是Seperater。


### misc notes

```
%comspec% /k "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
```

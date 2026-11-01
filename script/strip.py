import sys

def remove_cpp_comments(text):
    i = 0
    n = len(text)
    out = []
    state = 'NORMAL'  # 可选状态: NORMAL, LINE_COMMENT, BLOCK_COMMENT, STRING, CHAR
    
    while i < n:
        ch = text[i]
        
        if state == 'NORMAL':
            # 检查单行注释（包括 //, ///, //!）
            if i + 1 < n and text[i:i+2] == '//':
                state = 'LINE_COMMENT'
                i += 2
            # 检查块注释 / Doxygen 块注释（包括 /*, /**, /*!）
            elif i + 1 < n and text[i:i+2] == '/*':
                state = 'BLOCK_COMMENT'
                i += 2
            # 检查字符串起点，避免把字符串内的 // 或 /* 当成注释
            elif ch == '"':
                state = 'STRING'
                out.append(ch)
                i += 1
            # 检查字符常量起点
            elif ch == "'":
                state = 'CHAR'
                out.append(ch)
                i += 1
            else:
                out.append(ch)
                i += 1
                
        elif state == 'LINE_COMMENT':
            # 单行注释直到换行结束
            if ch == '\n':
                state = 'NORMAL'
                out.append(ch)  # 保留换行符，防止行号错乱
            i += 1
            
        elif state == 'BLOCK_COMMENT':
            # 块注释直到 */ 结束
            if i + 1 < n and text[i:i+2] == '*/':
                state = 'NORMAL'
                i += 2
            else:
                # 保留块注释内部的换行符，避免压缩代码
                if ch == '\n':
                    out.append(ch)
                i += 1
                
        elif state == 'STRING':
            out.append(ch)
            if ch == '\\' and i + 1 < n:
                # 处理转义字符，如 \"
                out.append(text[i+1])
                i += 2
            elif ch == '"':
                state = 'NORMAL'
                i += 1
            else:
                i += 1
                
        elif state == 'CHAR':
            out.append(ch)
            if ch == '\\' and i + 1 < n:
                # 处理转义字符，如 \'
                out.append(text[i+1])
                i += 2
            elif ch == "'":
                state = 'NORMAL'
                i += 1
            else:
                i += 1
                
    return "".join(out)

if __name__ == '__main__':
    content = sys.stdin.read()
    cleaned_content = remove_cpp_comments(content)
    sys.stdout.write(cleaned_content)
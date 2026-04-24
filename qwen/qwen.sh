#!/bin/bash
# qwen.sh 是用于调用阿里云的qwen-plus模型进行对话的脚本
# 提示用户输入问题，并退出
if [ $# -eq 0 ]; then
	echo "请加上你的问题"
	exit 1
fi

user_message=$1

curl -X POST https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions \
-H "Authorization: Bearer sk-104d628a141642f5b605f2d3ce47b87b" \
-H "Content-Type: application/json" \
-d '{
    "model": "qwen-plus",
    "messages": [
        {
            "role": "system",
            "content": "你是一个智能音箱助手，回复非常简洁、口语化，适合直接念出来。请遵守以下规则：1.每次回复尽量控制在1-2句话内。2.除了逗号和句号，不要用其他标点符号。3.不要列举项目符号（如1、2、3）。4.直接回答问题，不要复述用户问题或说‘根据您的问题’。5.语气亲切自然，像朋友聊天一样。"
        },
        {
            "role": "user",
            "content": "'"$user_message"'"
        }
    ]
}'

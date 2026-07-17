// todo:
// 1. 左侧消息列表选中需要有颜色颜色，且需要返回历史问答记录 -- 4.20 done
// 2. 欢迎界面的发送按钮要直接开启问答聊天界面，且把消息发送出去 -- 4.20 done
// 3. AI返回的答案要支持重新生成
// 4. ai的回答右上角需要有一个查看全部的按键，且点击需要有一个弹窗出来

// todo: 待优化
// 1. 输入框默认显示多行，且发送按钮一直居于右下角
// 2.

// todo：(可优化, 但不紧急)
// 1. 回答按照逐字显示的风格

interface chatMessage {
  sender: string;
  content: string;
  time: Date;
}

enum Sender {
  USER = 'user',
  ASSISTANT = 'assistant'
}

class ChatItem {
  id: number
  title: string
  // 消息数组
  messages: Array<chatMessage> = []
  lastActive: Date

  constructor() {
    this.id = Math.random()
    this.title = new Date().toLocaleTimeString()
    this.lastActive = new Date()
  }

  updateChatMsg(msgArr: Array<chatMessage>): ChatItem {
    if (msgArr === undefined || msgArr.length === 0) {
      return this
    }

    const copy = new ChatItem()
    copy.id = this.id
    copy.title = this.title
    copy.messages = msgArr
    copy.lastActive = new Date()
    return copy
  }

  updateChatTitle(title: string): ChatItem {
    if (title === undefined || !title.trim()) {
      return this
    }

    const copy = new ChatItem()
    copy.id = this.id
    copy.title = title
    copy.messages = [...this.messages]
    copy.lastActive = new Date()
    return copy
  }
}

// 欢迎面板组件
@Component
struct WelcomePanel {
  @State private inputText: string = ''
  onStartChat: (text: string) => void = () => {}

  build() {
    Column() {
      Text('你好这是你的问答助手')
        .fontSize(24)
        .fontWeight(FontWeight.Bold)
        .margin({ bottom: 30 })

      TextArea({ text: $$this.inputText, placeholder: '请输入您的问题...' })
        .width('80%')
        .height(50)
        .margin({ bottom: 20 })
        .onChange((value: string) => {
          this.inputText = value
        })
        .onSubmit(() => {
          if (this.inputText.trim()) {
            this.onStartChat(this.inputText)
          }
        })

      Button('开始对话')
        .width(200)
        .onClick(() => {
          if (this.inputText.trim()) {
            this.onStartChat(this.inputText)
          }
        })
    }
    .width('100%')
    .height('100%')
    .justifyContent(FlexAlign.Center)
    .alignItems(HorizontalAlign.Center)
  }
}

// 聊天面板组件
@Component
struct ChatPanel {
  @Link chatItem: ChatItem
  onSend: (text: string) => void = () => {}
  @State inputText: string = ''
  // 输入框的初始高度
  inputHeight: number = 50
  // 用于触发滚动
  private scroller: Scroller = new Scroller()
  scrollToBottom: boolean = false

  @Builder
  renderMessageItem(message: chatMessage) {
    Row() {
      if (message.sender === Sender.ASSISTANT) {
        this.renderAIAvatar()
        Blank().width(8) // 添加间距
      } else {
        Blank().layoutWeight(1)
      }

      Column() {
        Text(message.content)
          .fontSize(16)
          .textAlign(message.sender === Sender.USER ? TextAlign.End : TextAlign.Start)
          .backgroundColor(message.sender === Sender.USER ? '#409EFF' : '#F5F5F5')
          .padding(10)
          .borderRadius(10)
          .fontColor(message.sender === Sender.USER ? Color.White : Color.Black)

        Text(message.time.toLocaleTimeString())
          .fontSize(12)
          .fontColor('#999999')
          .margin({ top: 4 })
          .alignSelf(message.sender === Sender.USER ? ItemAlign.End : ItemAlign.Start)
      }
      .layoutWeight(6)
      .margin({
        left: message.sender === Sender.ASSISTANT ? 0 : 8,
        right: message.sender === Sender.USER ? 0 : 8
      })
      .alignItems(message.sender === Sender.USER ? HorizontalAlign.End : HorizontalAlign.Start)

      if (message.sender === Sender.USER) {
        this.renderUserAvatar()
      } else {
        Blank().layoutWeight(1)
      }
    }
    .width('100%')
    .margin({ top: 8, bottom: 8 })
    .justifyContent(message.sender === Sender.USER ? FlexAlign.End : FlexAlign.Start)
  }

  @Builder
  renderAIAvatar() {
    Image($r('app.media.arrow_up_circle_fill')) // 确保这个资源存在
      .width(40)
      .height(40)
      .borderRadius(20)
      .margin({ right: 8 })
  }

  @Builder
  renderUserAvatar() {
    Image($r('app.media.person_crop_circle_fill_1'))
      .width(40)
      .height(40)
      .borderRadius(20)
      .margin({ left: 10 })
  }

  build() {
    Column() {
      // 标题
      Row() {
        Text(this.chatItem?.title)
          .maxLines(1)
          .textAlign(TextAlign.Center)
          .fontSize(50)
          .fontWeight(FontWeight.Bold)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
      }

      // 问答显示区域
      Column() {
        List() {
          ForEach(this.chatItem?.messages, (message: chatMessage) => {
            ListItem() {
              this.renderMessageItem(message)
            }
          }, (message: chatMessage) => `${message.time.getTime()}`)
        }
        .onReachEnd(() => {
          this.scrollToBottom = true
        })
        .scrollBar(BarState.Auto)
        .onAppear(() => {
          if (this.scrollToBottom) {
            this.scroller.scrollToIndex(this.chatItem.messages.length - 1)
          }
        })
        .width('100%')
        .height('100%')
        Text('').width(1).height(1) // 底部占位
      }
      .width('100%')
      .borderColor(Color.Blue)
      .layoutWeight(1)


      // 输入区域
      Column() {
        // 输入框区域
        Row() {
          // 文本输入框
          TextArea({
            text: $$this.inputText,
            placeholder: '输入消息...',
          })
            .width('100%')
            .height(this.inputHeight)
            .backgroundColor('#f5f5f5')
            .borderRadius(20)
            .padding({ left: 16, right: 48, top: 12, bottom: 12 })  // 右侧留出图标位置
            .fontSize(16)
            .fontColor('#333333')
            .enterKeyType(EnterKeyType.Send)
            .onChange((value: string) => {
              this.inputText = value
              // 动态调整高度（基础高度56 + 每行增加20）
              const lineCount = value.split('\n').length
              this.inputHeight = Math.min(56 + (lineCount - 1) * 20, 150)  // 最大高度150
            })
            .onSubmit(() => {  // 回车键提交
              console.log("on submit call: ", this.inputText)
              if (this.inputText.trim()) {
                this.onSend(this.inputText)
                this.inputText = ''
                this.inputHeight = 56
              }
            })

          // 发送按钮
          Button() {
            Image($r('app.media.arrow_up_circle_fill'))
              .width(24)
              .height(24)
              .fillColor(Color.Blue)
          }
          .width(40)
          .height(40)
          .position({ x: '100%' })
          .margin({ right: 8 })
          .translate({ x: -48 })
          .backgroundColor(Color.Transparent)
          .onClick(() => {
            this.onSend(this.inputText)
            this.inputText = ''
            this.inputHeight = 56
          })
        }
      }
      .width('100%')
      .padding(12)
      .backgroundColor(Color.White)
      .border({ width: 1, color: '#EEEEEE' })
      .borderRadius(24)
      .margin({ top: 16 })
    }
    .width('100%')
    .height('100%')
  }
}

// 聊天列表项组件
@Component
struct ChatListItem {
  @Prop chatItem: ChatItem
  @Prop isActive: boolean
  onSend: (chat: ChatItem) => void = () => {}
  onDelete: (chatId: number) => void = () => {}
  onRename: (chatId: number, newTitle: string) => void = () => {}

  // 编辑选项
  @State private showMenu: boolean = false
  @State private editingTitle: boolean = false
  @State private newTitle: string = ''

  aboutToAppear() {
    this.newTitle = this.chatItem.title
  }

  @Builder
  renderMoreIcon() {
    Image($r('app.media.arrow_up_circle_fill')) // 确保这个资源存在
      .width(20)
      .height(20)
      .objectFit(ImageFit.Contain)
  }

  build() {
    Column() {
      Row() {
        // 左侧内容区域
        Column() {
          if (this.editingTitle) {
            TextInput({placeholder: '请输入新的标题...'})
              .fontSize(16)
              .onChange((value: string) => {
                this.newTitle = value
              })
              .onSubmit(() => {
                if (this.newTitle.trim()) {
                  this.onRename(this.chatItem.id, this.newTitle)
                }
                this.editingTitle = false
              })
          } else {
            Text(this.chatItem.title)
              .fontSize(16)
              .fontColor(this.isActive ? '#409EFF' : '#333333')
              .maxLines(1)
              .textOverflow({ overflow: TextOverflow.Ellipsis })
          }

          Text(this.chatItem.lastActive.toLocaleTimeString())
            .fontSize(12)
            .fontColor(this.isActive ? '#409EFF' : '#666666')
        }
        .layoutWeight(1)
        .onClick(() => {
          if (!this.isActive) {
            this.onSend(this.chatItem)
          }
        })

        // 右侧操作按钮 (仅在选中时显示)
        if (this.isActive) {
          Row() {
            this.renderMoreIcon() // 使用图标代替文字按钮
          }
          .margin({ right: 12 })
          .onClick(() => {
            this.showMenu = !this.showMenu
          })
        }
      }
      .width('100%')
      .padding(40)
      .backgroundColor(this.isActive ? '#F0F7FF' : 'transparent')
      .backgroundColor(this.isActive ? '#F0F7FF' : 'transparent')
      .border({
        width: this.isActive ? 2 : 0,  // 选中时显示2px边框，未选中时不显示
        color: this.isActive ? '#409EFF' : Color.Transparent  // 边框颜色与选中状态一致
      })
      .borderRadius(8)  // 圆角边框
      .margin({ top: 4, bottom: 4, left: 10, right: 10})  // 增加上下间距避免边框重叠
      .alignItems(VerticalAlign.Center)

      // 操作菜单 (下拉菜单)
      if (this.isActive && this.showMenu) {
        Column() {
          Button('重命名', { type: ButtonType.Normal })
            .width(80)
            .height(30)
            .onClick(() => {
              this.editingTitle = true
              this.showMenu = false
            })

          Button('删除', { type: ButtonType.Normal })
            .width(80)
            .height(30)
            .margin({ top: 5 })
            .onClick(() => {
              this.onDelete(this.chatItem.id)
              this.showMenu = false
            })
        }
        .width(100)
        .padding(5)
        .backgroundColor(Color.White)
        .border({ width: 1, color: '#EEEEEE' })
        .borderRadius(4)
        .position({ x: '80%', y: 50 })
        .zIndex(1)
      }
    }
  }
}

@Entry
@Component
struct Index {
  // 当前聊天
  @State currentChat?: ChatItem | undefined = undefined
  // 聊天列表
  @State chatList: Array<ChatItem> = []
  // 输入框内容
  @State inputText: string = ''
  // 是否显示欢迎界面
  @State showWelcome: boolean = true

  aboutToAppear() {
    // 如果没有聊天记录，显示欢迎界面
    this.showWelcome = this.chatList.length === 0
  }

  @Builder
  leftChatHistory() {
    Column() {
      // 发送消息的按钮
      Button() {
        Row() {
          Text('+')
            .fontSize(20)
          Text('新建对话')
            .fontSize(14)
            .margin({ left: 8 })
        }
        .width('100%')
        .justifyContent(FlexAlign.Center)
        .height(50)
        .alignItems(VerticalAlign.Center)
      }
      .width('90%')
      .margin(10)
      .onClick(() => {
        this.createNewChat()
      })

      // 历史聊天列表
      List({ space: 10 }) {
        ForEach(this.chatList, (item: ChatItem) => {
          ListItem() {
            ChatListItem({
              chatItem: item,
              isActive: this.currentChat?.id === item.id,
              onSend: this.selectChat,
              onDelete: this.deleteChat,
              onRename: this.renameChat
            })
          }
        }, (item: ChatItem) => `${item.id}-${item.title}`)
      }
      .width('100%')
      .height('100%')
      .layoutWeight(1)
    }
    .width("30%")
    .height('100%')
    .borderWidth(1)
    .borderStyle(BorderStyle.Solid)
    .borderColor(Color.Blue)
    .borderRadius(30)
  }

  @Builder
  rightChat() {
    Column() {
      if (this.showWelcome || this.chatList.length === 0) {
        // 欢迎界面
        WelcomePanel({
          onStartChat: (text: string) => {
            this.createNewChat(text)
            this.sendMessage(text)
          }
        })
      } else {
        // 确保有有效的聊天项
        if (this.currentChat) {
          ChatPanel({
            chatItem: this.currentChat,
            onSend: this.sendMessage,
            inputText: this.inputText
          })
        } else {
          // 作为后备，显示欢迎面板
          WelcomePanel({
            onStartChat: (text: string) => {
              this.createNewChat(text)
              this.sendMessage(text)
            }
          })
        }
      }
    }
    .backgroundColor('#ADD8E6')
    .width("70%")
    .height('100%')
    .borderWidth(1)
    .borderStyle(BorderStyle.Solid)
    .borderColor(Color.Blue)
    .borderRadius(30)
  }

  build() {
    Row() {
      // 左边的消息框
      this.leftChatHistory()

      // 右边的聊天框和欢迎框
      this.rightChat()
    }
  }

  // 创建新聊天
  private createNewChat = (title?: string): void => {
    if (title) {
      const newChat = new ChatItem()
      newChat.title = title
      this.chatList = [newChat, ...this.chatList]
      this.currentChat = newChat
      this.showWelcome = false
    } else {
      // 如果没有传入标题，就显示欢迎界面
      this.showWelcome = true
      this.currentChat = undefined
    }
    this.inputText = ''
  }

  // 选择聊天
  private selectChat = (chat: ChatItem): void => {
    const selectedChat = this.chatList.find(item => item.id === chat.id);
    if (selectedChat) {
      this.currentChat = selectedChat;
      this.showWelcome = false;

      console.log("select chat call, chat id: ", chat.id)

      // 强制更新聊天列表以反映选择变化
      this.chatList = [...this.chatList];
    }
  }

  // 删除聊天记录
  private deleteChat = (chatId: number): void => {
    this.chatList = this.chatList.filter(item => item.id !== chatId);
    if (this.currentChat?.id === chatId) {
      // 判断被删的是不是第一个, 是的话切换回欢迎界面
      this.currentChat = this.chatList[0] || undefined;
      this.showWelcome = this.chatList.length === 0;
    }
  }

  // 重命名聊天记录
  private renameChat = (chatId: number, newTitle: string): void => {
    if (!newTitle.trim()) {
      return
    }

    // 1. 更新聊天列表
    this.chatList = this.chatList.map((item: ChatItem): ChatItem => {
      if (item.id === chatId) {
        const updated = item.updateChatTitle(newTitle);
        // 如果正在编辑的是当前聊天，也更新currentChat
        if (this.currentChat?.id === chatId) {
          this.currentChat = updated;
        }
        return updated;
      }
      return item;
    });

    console.log('update new title', this.currentChat?.title)
    // 2. 强制刷新UI
    this.chatList = [...this.chatList]
  }

  // 按最后活动时间排序聊天列表
  private sortChatList = (): void => {
    this.chatList.sort((a, b) => b.lastActive.getTime() - a.lastActive.getTime())
  }

  private sendMessage = (text: string): void => {
    if (!text.trim() ||!this.currentChat) {
      return
    }

    const currentChat = this.currentChat
    const userMessage: chatMessage = {
      sender: Sender.USER,
      content: text,
      time: new Date()
    }

    this.currentChat = currentChat.updateChatMsg([...currentChat.messages, userMessage]);
    this.chatList = this.chatList.map(item => item.id === currentChat.id? this.currentChat! : item)

    this.sortChatList()
    this.inputText = ''

    // 模拟AI回复流程
    setTimeout(() => {
      const aiResponse = "这是AI的回答"
      const aiMessage: chatMessage = {
        sender: Sender.ASSISTANT,
        content: aiResponse,
        time: new Date()
      };

      this.currentChat = this.currentChat!.updateChatMsg([...this.currentChat!.messages, aiMessage]);
      this.chatList = this.chatList.map(item => item.id === this.currentChat!.id? this.currentChat! : item)
    }, 1000)
  }
}
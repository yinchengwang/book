const INVALID_SESSION_ID = -1;
const INVALID_CHAT_ID = -1;

class chatInfo {
  chatId: number = INVALID_CHAT_ID;
  question: string = "";
  answer: string = "";
  reference: Array<number> = [];
  updateTime: number = 0;
}

class mailInfo {
  mailId: number = 0;
  subject: string = ""
  receivedTime: number = 0;
}

class sessionInfo {
  sessionId: number = INVALID_SESSION_ID;
  title: string = "";
  updateTime: number = 0;
  chatInfoList: Array<chatInfo> = [];
  mailInfoList: Array<mailInfo> = [];
}

@Component
struct showSessionInfo {
  @Prop currSessionId: number;
  @Prop sessionList: Array<sessionInfo>;

  build() {
    Column() {
      Text(`session(${this.currSessionId}) Information: `)
      List() {
        ForEach(this.sessionList, (item: sessionInfo) => {
          ListItem() {
            if (item.sessionId === this.currSessionId) {
              Column() {
                Text(`session id: ${item.sessionId}`);
                Text(`session title: ${item.title}`);
                Text(`session update time: ${item.updateTime}`);
              }
              .width("100%")
              .alignItems(HorizontalAlign.Center)
            }
          }
        })
      }
      .height("auto")
      .width("100%")
    }
    .backgroundColor(Color.Pink)
    .width("100%")
    .alignItems(HorizontalAlign.Center)
  }
}

@Component
struct showSessionChatInfo {
  @Prop currSessionId: number;
  @Prop sessionList: Array<sessionInfo>;

  build() {
    Column() {
      Text(`session(${this.currSessionId})'s chat Information: `)
      List() {
        ForEach(this.sessionList, (item: sessionInfo) => {
          ListItem() {
            if (item.sessionId === this.currSessionId) {
              Column() {
                ForEach(item.chatInfoList, (item: chatInfo) => {
                  List() {
                    ListItem() {
                      Column() {
                        Text(`chat id: ${item.chatId}`);
                        Text(`chat answer: ${item.answer}`);
                        Text(`chat question: ${item.question}`);
                        Text(`chat reference length: ${item.reference.length}`);
                        Text(`chat updateTime: ${item.updateTime}`);
                      }
                      .width("100%")
                      .alignItems(HorizontalAlign.Center)
                    }
                  }
                  .height("auto")
                  .width("100%")
                })
              }
            }
          }
        })
      }
      .height("auto")
      .width("100%")
    }
    .backgroundColor(Color.Yellow)
    .width("100%")
    .alignItems(HorizontalAlign.Center)
  }
}

@Component
struct showSessionMailInfo {
  @Prop currSessionId: number;
  @Prop sessionList: Array<sessionInfo>;

  build() {
    Column() {
      Text(`session(${this.currSessionId})'s chat Information: `)
      List() {
        ForEach(this.sessionList, (item: sessionInfo) => {
          ListItem() {
            if (item.sessionId === this.currSessionId) {
              Column() {
                ForEach(item.mailInfoList, (item: mailInfo) => {
                  List() {
                    ListItem() {
                      Column() {
                        Text(`mail id: ${item.mailId}`);
                        Text(`mail subject: ${item.subject}`);
                        Text(`mail receivedTime: ${item.receivedTime}`);
                      }
                      .width("100%")
                      .alignItems(HorizontalAlign.Center)
                    }
                  }
                  .height("auto")
                  .width("100%")
                })
              }
            }
          }
        })
      }
      .height("auto")
      .width("100%")
    }
    .backgroundColor(Color.Orange)
    .width("100%")
    .alignItems(HorizontalAlign.Center)
  }
}

@Entry
@Component
struct Index {
  @State currSessionId: number = 0;
  @State sessionList: Array<sessionInfo> = [];

  aboutToAppear(): void {
    this.sessionInfoInit(3);
  }

  private chatInfoInit(totalChat: number): Array<chatInfo> {
    let chatList: Array<chatInfo> = [];
    for (let idx = 0; idx < totalChat; idx++) {
      let chat: chatInfo = new chatInfo();
      chat.chatId = idx;
      chat.question = "这是问题 " + String(idx);
      chat.answer = "这是答案 " + String(idx);
      chat.reference = [];
      chat.updateTime = idx;

      chatList.push(chat);
    }

    return chatList;
  }

  private mailInfoInit(totalMail: number): Array<mailInfo> {
    let mailList: Array<mailInfo> = [];
    for (let idx = 0; idx < totalMail; idx++) {
      let mail: mailInfo = new mailInfo();
      mail.mailId = idx;
      mail.subject = "这是邮件标题 " + String(idx);
      mail.receivedTime = idx;

      mailList.push(mail);
    }

    return mailList;
  }

  private sessionInfoInit(totalSession: number) {
    let sessionList: Array<sessionInfo> = [];
    for (let idx = 0; idx < totalSession; idx++) {
      let session = new sessionInfo();
      session.sessionId = idx;
      session.title = "session title " + String(idx);
      session.updateTime = idx * 2;
      session.chatInfoList = this.chatInfoInit(idx + 1);
      session.mailInfoList = this.mailInfoInit(idx + 1);

      sessionList.push(session);
    }

    this.sessionList = sessionList;
    this.currSessionId = 0;
  }

  private insertSessionInfo() {
    let newSessionId: number = this.sessionList.length;

    let newSession: sessionInfo = new sessionInfo();
    newSession.sessionId = newSessionId;
    newSession.title = "session title " + String(newSessionId);
    newSession.updateTime = newSessionId * 2;
    newSession.chatInfoList = this.chatInfoInit(newSessionId + 1);
    newSession.mailInfoList = this.mailInfoInit(newSessionId + 1);

    this.sessionList = [...this.sessionList, newSession];
    this.currSessionId = newSessionId;
  }

  private updateSessionChatQuestion() {
    let session = this.sessionList[this.currSessionId];
    session.chatInfoList[0].question = session.chatInfoList[0].question + "这是一个新问题呢"

    this.sessionList = this.sessionList.map((item: sessionInfo) => {
      if (item.sessionId === this.currSessionId) {
        return session;
      } else {
        return item;
      }
    })
  }

  private updateSessionMailInfo() {
    let session = this.sessionList[this.currSessionId];
    session.mailInfoList[0].subject = session.mailInfoList[0].subject + "这是一个新的主题呢"

    this.sessionList = this.sessionList.map((item: sessionInfo) => {
      if (item.sessionId === this.currSessionId) {
        return session;
      } else {
        return item;
      }
    })
  }

  build() {
    Column({space: 10}) {
      Row() {
        // 更新chat信息
        Button() {
          Text("update Chat question")
            .fontSize(14)
        }
        .height(50)
        .type(ButtonType.ROUNDED_RECTANGLE)
        .onClick(() => {
          this.updateSessionChatQuestion();
        })

        // 更新mail信息
        Button() {
          Text("update mail subject")
            .fontSize(14)
        }
        .height(50)
        .type(ButtonType.ROUNDED_RECTANGLE)
        .onClick(() => {
          this.updateSessionMailInfo();
        })
      }
      .justifyContent(FlexAlign.SpaceBetween)
      .height(50)
      .width("100%")

      Row() {
        // 切换对话
        Button() {
          Text("switch session")
            .fontSize(14)
        }
        .height(50)
        .type(ButtonType.ROUNDED_RECTANGLE)
        .onClick(() => {
          const nextSessionId = this.currSessionId + 1;
          this.currSessionId = nextSessionId >= this.sessionList.length ? 0 : nextSessionId;
        })

        // 新增对话
        Button() {
          Text("insert session")
            .fontSize(14)
        }
        .height(50)
        .type(ButtonType.ROUNDED_RECTANGLE)
        .onClick(() => {
          this.insertSessionInfo();
        })
      }
      .width("100%")
      .justifyContent(FlexAlign.SpaceBetween)

      Scroll() {
        Column() {
          Text(`currSessionId: {${this.currSessionId}}`)
            .textAlign(TextAlign.Center)
            .width("100%")
            .backgroundColor(Color.Red)
          // session信息
          showSessionInfo({
            sessionList: this.sessionList,
            currSessionId: this.currSessionId
          })
          // session的chat信息
          showSessionChatInfo({
            sessionList: this.sessionList,
            currSessionId: this.currSessionId
          })
          // session的mail信息
          showSessionMailInfo({
            sessionList: this.sessionList,
            currSessionId: this.currSessionId
          })
        }
        .alignItems(HorizontalAlign.Center)
      }
      .width("100%")
      .layoutWeight(1)
      .scrollBar(BarState.On)
    }
    .height('100%')
    .width('100%')
  }
}
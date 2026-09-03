// Taro 入口页面
import { Component, PropsWithChildren } from 'react';

class Index extends Component<PropsWithChildren> {
  render() {
    return this.props.children;
  }
}

export default Index;

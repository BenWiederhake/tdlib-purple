#include "fixture.h"
#include "libpurple-mock.h"
#include <fmt/format.h>

class MessageOrderTest: public CommTest {};

TEST_F(MessageOrderTest, ReplyOrdering)
{
    const int32_t dates[2]  = {10002, 10003};
    const int64_t msgIds[2] = {2, 3};
    const int32_t srcDate  = 10001;
    const int64_t srcMsgId = 1;
    loginWithOneContact();

    object_ptr<message> message = makeMessage(
        msgIds[0], userIds[0], chatIds[0], false, dates[0], makeTextMessage("reply")
    );
    message->reply_to_message_id_ = srcMsgId;

    tgl.update(make_object<updateNewMessage>(std::move(message)));
    uint64_t getMessageReqId = tgl.verifyRequest(
        getMessage(chatIds[0], srcMsgId)
    );
    prpl.verifyNoEvents();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        msgIds[1], userIds[0], chatIds[0], false, dates[1], makeTextMessage("followUp")
    )));
    prpl.verifyNoEvents();

    tgl.reply(getMessageReqId, makeMessage(srcMsgId, userIds[0], chatIds[0], false, srcDate, makeTextMessage("original")));
    prpl.verifyEvents(
        ServGotImEvent(
            connection, purpleUserName(0),
            fmt::format(replyPattern, userFirstNames[0] + " " + userLastNames[0], "original", "reply"),
            PURPLE_MESSAGE_RECV, dates[0]
        ),
        ServGotImEvent(connection, purpleUserName(0), "followUp", PURPLE_MESSAGE_RECV, dates[1])
    );
    tgl.verifyRequest(viewMessages(chatIds[0], {msgIds[0], msgIds[1]}, true));
}

TEST_F(MessageOrderTest, Reply_FlushAtLogout)
{
    const int32_t dates[2]  = {10002, 10003};
    const int64_t msgIds[2] = {2, 3};
    const int64_t srcMsgId  = 1;
    loginWithOneContact();

    object_ptr<message> message = makeMessage(
        msgIds[0], userIds[0], chatIds[0], false, dates[0], makeTextMessage("reply")
    );
    message->reply_to_message_id_ = srcMsgId;

    tgl.update(make_object<updateNewMessage>(std::move(message)));
    tgl.verifyRequest(getMessage(chatIds[0], srcMsgId));
    prpl.verifyNoEvents();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        msgIds[1], userIds[0], chatIds[0], false, dates[1], makeTextMessage("followUp")
    )));
    prpl.verifyNoEvents();

    pluginInfo().close(connection);
    prpl.verifyEvents(
        ServGotImEvent(
            connection, purpleUserName(0),
            fmt::format(replyPattern, "Unknown user", "[message unavailable]", "reply"),
            PURPLE_MESSAGE_RECV, dates[0]
        ),
        ServGotImEvent(connection, purpleUserName(0), "followUp", PURPLE_MESSAGE_RECV, dates[1])
    );
    tgl.verifyRequest(viewMessages(chatIds[0], {msgIds[0], msgIds[1]}, true));
}

TEST_F(MessageOrderTest, Photo_Download_FlushAtLogout)
{
    const int32_t date   = 10001;
    const int32_t fileId = 1234;
    loginWithOneContact();

    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, 10000, 10000,
            make_object<localFile>("", true, true, false, false, 0, 0, 0),
            make_object<remoteFile>("beh", "bleh", false, true, 10000)
        ),
        640, 480
    ));
    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messagePhoto>(
            make_object<photo>(false, nullptr, std::move(sizes)),
            make_object<formattedText>("photo", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequest(downloadFile(fileId, 1, 0, 0, true));
    prpl.verifyNoEvents();

    pluginInfo().close(connection);
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "photo", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Downloading photo",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );
    tgl.verifyRequest(viewMessages(chatIds[0], {1}, true));
}

class MessageOrderTestLongDownloadInReply: public MessageOrderTest,
    public testing::WithParamInterface<std::string> {};

TEST_P(MessageOrderTestLongDownloadInReply, LongDownloadInReply)
{
    const int32_t date     = 10002;
    const int64_t msgId    = 2;
    const int32_t fileId   = 1234;
    const int32_t srcDate  = 10001;
    const int64_t srcMsgId = 1;
    std::string   caption  = GetParam();
    loginWithOneContact();

    object_ptr<message> message = makeMessage(
        msgId, userIds[0], chatIds[0], false, date,
        make_object<messageDocument>(
            make_object<document>(
                "doc.file.name", "mime/type", nullptr, nullptr,
                make_object<file>(
                    fileId, 10000, 10000,
                    make_object<localFile>("", true, true, false, false, 0, 0, 0),
                    make_object<remoteFile>("beh", "bleh", false, true, 10000)
                )
            ),
            make_object<formattedText>(caption, std::vector<object_ptr<textEntity>>())
        )
    );
    message->reply_to_message_id_ = srcMsgId;

    tgl.update(make_object<updateNewMessage>(std::move(message)));
    auto requestIds = tgl.verifyRequests({
        make_object<getMessage>(chatIds[0], srcMsgId),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    uint64_t getMessageReqId = requestIds.at(0);
    uint64_t downloadReqId = requestIds.at(1);
    prpl.verifyNoEvents();

    tgl.reply(getMessageReqId, makeMessage(srcMsgId, userIds[0], chatIds[0], false, srcDate, makeTextMessage("1<2")));

    runTimeouts();
    std::string tempFileName;
    if (!caption.empty())
        prpl.verifyEvents(
            XferAcceptedEvent(purpleUserName(0), &tempFileName),
            ServGotImEvent(
                connection, purpleUserName(0),
                fmt::format(replyPattern, userFirstNames[0] + " " + userLastNames[0], "1&lt;2", caption),
                PURPLE_MESSAGE_RECV, date
            ),
            ConversationWriteEvent(
                purpleUserName(0), purpleUserName(0),
                userFirstNames[0] + " " + userLastNames[0] + ": Downloading doc.file.name [mime/type]",
                PURPLE_MESSAGE_SYSTEM, date
            )
        );
    else
        prpl.verifyEvents(
            XferAcceptedEvent(purpleUserName(0), &tempFileName),
            NewConversationEvent(PURPLE_CONV_TYPE_IM, account, purpleUserName(0)),
            ConversationWriteEvent(
                purpleUserName(0), purpleUserName(0),
                userFirstNames[0] + " " + userLastNames[0] + ": Downloading doc.file.name [mime/type]",
                PURPLE_MESSAGE_SYSTEM, date
            )
        );
    tgl.verifyRequest(viewMessages(chatIds[0], {msgId}, true));

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, true, false, 0, 0, 2000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));
    prpl.verifyEvents(
        XferStartEvent(tempFileName),
        XferProgressEvent(tempFileName, 2000)
    );

    tgl.reply(downloadReqId, make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));
    prpl.verifyEvents(
        XferCompletedEvent(tempFileName, TRUE, 10000),
        XferEndEvent(tempFileName),
        ServGotImEvent(
            connection, purpleUserName(0),
            fmt::format(replyPattern, userFirstNames[0] + " " + userLastNames[0], "1&lt;2", 
                        "<a href=\"file:///path\">doc.file.name [mime/type]</a>"),
            PURPLE_MESSAGE_RECV, date
        )
    );
    ASSERT_FALSE(g_file_test(tempFileName.c_str(), G_FILE_TEST_EXISTS));
}

INSTANTIATE_TEST_CASE_P(bleh, MessageOrderTestLongDownloadInReply, ::testing::Values("", "caption"));

TEST_F(MessageOrderTest, DownloadOrdering)
{
    const int64_t messageId[3] = {1, 2, 3};
    const int32_t date[3]      = {10001, 10002, 10003};
    const int32_t fileId[3]    = {1234, 0, 1235};
    loginWithOneContact();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId[0], userIds[0], chatIds[0], false, date[0],
        make_object<messageDocument>(
            make_object<document>(
                "doc1.file.name", "mime/type", nullptr, nullptr,
                make_object<file>(
                    fileId[0], 10000, 10000,
                    make_object<localFile>("", true, true, false, false, 0, 0, 0),
                    make_object<remoteFile>("beh", "bleh", false, true, 10000)
                )
            ),
            make_object<formattedText>("document1", std::vector<object_ptr<textEntity>>())
        )
    )));
    uint64_t download1ReqId = tgl.verifyRequest(
        downloadFile(fileId[0], 1, 0, 0, true)
    );
    prpl.verifyNoEvents();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId[1], userIds[0], chatIds[0], false, date[1], makeTextMessage("followUp")
    )));
    prpl.verifyNoEvents();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId[2], userIds[0], chatIds[0], false, date[2],
        make_object<messageDocument>(
            make_object<document>(
                "doc2.file.name", "mime/type", nullptr, nullptr,
                make_object<file>(
                    fileId[2], 10000, 10000,
                    make_object<localFile>("", true, true, false, false, 0, 0, 0),
                    make_object<remoteFile>("beh", "bleh", false, true, 10000)
                )
            ),
            make_object<formattedText>("document1", std::vector<object_ptr<textEntity>>())
        )
    )));
    uint64_t download2ReqId = tgl.verifyRequest(
        downloadFile(fileId[2], 1, 0, 0, true)
    );
    prpl.verifyNoEvents();

    tgl.reply(download1ReqId, make_object<file>(
        fileId[0], 10000, 10000,
        make_object<localFile>("/path1", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));
    prpl.verifyEvents(
        ServGotImEvent(
            connection, purpleUserName(0),
            "<a href=\"file:///path1\">doc1.file.name [mime/type]</a>\ndocument1",
            PURPLE_MESSAGE_RECV, date[0]
        ),
        ServGotImEvent(connection, purpleUserName(0), "followUp", PURPLE_MESSAGE_RECV, date[1])
    );
    // TODO: third read receipt is technically premature but who cares
    tgl.verifyRequest(viewMessages(chatIds[0], {messageId[0], messageId[1], messageId[2]}, true));

    tgl.reply(download2ReqId, make_object<file>(
        fileId[0], 10000, 10000,
        make_object<localFile>("/path2", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));
    prpl.verifyEvents(
        ServGotImEvent(
            connection, purpleUserName(0),
            "<a href=\"file:///path2\">doc2.file.name [mime/type]</a>\ndocument1",
            PURPLE_MESSAGE_RECV, date[2]
        )
    );
}

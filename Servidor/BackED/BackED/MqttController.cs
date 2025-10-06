using System.Text;
using MQTTnet.Protocol;
using MQTTnet.Server;

namespace BackFanPlate;

public class MqttController
{
    public MqttController() { }

    public static Task ClientDisconnectedAsync(ClientDisconnectedEventArgs arg)
    {
        Console.WriteLine($"SERVER MQTT: Client disconnected '{arg.ClientId}'");
        return Task.CompletedTask;
    }

    public static Task ValidatingConnectionAsync(ValidatingConnectionEventArgs arg)
    {
        Console.WriteLine($"SERVER MQTT: Client connected '{arg.ClientId}'");
        arg.ReasonCode = MqttConnectReasonCode.Success;
        return Task.CompletedTask;
    }

    public static Task InterceptingPublishAsync(InterceptingPublishEventArgs arg)
    {
        Console.WriteLine($"SERVER MQTT: Client '{arg.ClientId}', '{arg.ApplicationMessage.Topic}' : {Encoding.UTF8.GetString(arg.ApplicationMessage.Payload)}");
        
        return Task.CompletedTask;
    }

    public static Task InterceptingSubscriptionAsync(InterceptingSubscriptionEventArgs arg)
    {
        Console.WriteLine($"SERVER MQTT: Client '{arg.ClientId}' subscribed to topic '{arg.TopicFilter.Topic}'");
        return Task.CompletedTask;
    }

    public static Task InterceptingUnsubscriptionAsync(InterceptingUnsubscriptionEventArgs arg)
    {
        Console.WriteLine($"SERVER MQTT: Client '{arg.ClientId}' unsubscribed to topic '{arg.Topic}'");
        return Task.CompletedTask;
    }
}